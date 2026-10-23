// RE2 (Resident Evil 2 2019 remake) online-co-op protocol prototype, wrapped as a coopfuzz target.
// The model of the protocol we want to implement in the mod (REFramework Lua + C++ transport):
//
//   - 2 to 4 machines, machine 0 = the host (P1, the one who runs the campaign). The other machines
//     are the players with separate PCs.
//   - Pawns of players: each machine simulates its own pawn locally and sends its position
//     to the host with an increasing seq (retransmit until the host acks). The host tables out a "game
//     state" snapshot in "version"s: every machine's pawn, the zombies (host-authoritative),
//     doors, and world items (holder machine). All replicated with the SAME versioned snapshot:
//     idem = (version, thing) per thing, same dedupe rule as the skeleton: discard if version <= myVersion_.
//   - Doors/objects: the machine presses Interact (Activate) → request to the host; only the host
//     toggles its state, the rest see it in the snapshot.
//   - Items: Pickup = request → the host moves the item to that machine's holder; replicated in the snapshot.
//     Pickup of an already taken item is a clean reject (already taken) = no duplication.
//   - Rivals/zombies: the host simulates them and only the snapshot replicates them. Spawn (a site)
//     wakes a dormant rival; spawning a site twice is a reject (already spawned) = no duplication.
//   - Story (dial/stage): not reserved in this MVP (TODO); Save/Load are ignored here;
//     join/leave are not implemented (machines always connected): honest default documented.
//
// Instinct: this validates the SHAPE (host-authoritative versioned snapshot + client requests
// with acks/retries) before transcribing it to the mod. Bug twin available: "no_snapshot_dedupe".
#include <algorithm>
#include <functional>
#include <vector>
#include "coopfuzz/fuzz.h"

namespace coopfuzz {
namespace {

// ====================================================================== part 1: the sync core being tested
uint32_t sum32(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}
void put32(Bytes& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }
uint32_t get32(const Bytes& b, size_t at) { uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(b[at + static_cast<size_t>(i)]) << (8 * i); return v; }
void put8(Bytes& b, uint32_t v) { b.push_back(static_cast<uint8_t>(v & 0xFF)); }
uint8_t get8(const Bytes& b, size_t at) { return b[at]; }
int getInt(const Bytes& b, size_t at) { return static_cast<int>(get32(b, at)); }

constexpr int kMaxPlayers = 4;
constexpr int kEnemies = 4;
constexpr int kDoors = 4;
constexpr int kItems = 4;
constexpr uint32_t kPawnBase = 0x100;
constexpr uint32_t kEnemyBase = 0x200;
constexpr uint32_t kDoorBase = 0x300;
constexpr uint32_t kItemBase = 0x400;

struct V3 { int x = 0, y = 0; };

struct Zombie { bool alive = false; bool spawned = false; V3 pos; int hp = 100; };
struct DoorState { bool open = false; };
struct ItemState { int holder = -1; };   // -1 = on the ground

// The report the real core needs: one per message applied or rejected.
using ReportFn = std::function<void(int machine, uint64_t idem, int ref, uint32_t seq, int origin, ApplyKind kind, bool accepted, const char* why)>;

// Packet types
enum : uint8_t {
    kMove = 1,       // client pawn: seq, x, y                              (client -> host)
    kMoveAck = 2,    // up-to-seq ack                                       (host -> client)
    kSnap = 3,       // version, players, zombies, doors, items             (host -> client)
    kSnapAck = 4,    // ack of a version                                    (client -> host)
    kDoorReq = 5,    // door index                                          (client -> host)
    kPickReq = 6,    // item index                                          (client -> host)
    kSpawnReq = 7,   // site                                                (client -> host)
    kHitReq = 8,     // enemy index, damage                                 (client -> host)
};

class Re2Core {
public:
    Re2Core(int id, int players, bool dedupeSnapshots, std::function<void(int, Bytes)> send, ReportFn report)
        : id_(id), n_(players), dedupe_(dedupeSnapshots), send_(std::move(send)), report_(std::move(report)),
          pawnPos_(static_cast<size_t>(players)), lastSeq_(static_cast<size_t>(players), 0),
          snapAcked_(static_cast<size_t>(players), 0) {}

    const V3& pawnPos(int p) const { return pawnPos_[static_cast<size_t>(p)]; }
    const V3& zombiePos(int z) const { return zom_[static_cast<size_t>(z)].pos; }
    bool zombieAlive(int z) const { return zom_[static_cast<size_t>(z)].alive; }
    bool doorOpen(int d) const { return door_[static_cast<size_t>(d)].open; }
    int itemHolder(int it) const { return item_[static_cast<size_t>(it)].holder; }
    uint32_t myVersion() const { return myVersion_; }
    uint32_t hostVersion() const { return version_; }
    uint32_t snapAcked(int c) const { return snapAcked_[static_cast<size_t>(c)]; }
    uint32_t moveAcked() const { return ackedSeq_; }
    uint32_t localSeq() const { return localSeq_; }

    // -------- local actions (the game's own thread)

    void setLocalPawnPosition(int x, int y) {   // "my pawn moved"
        pawnPos_[static_cast<size_t>(id_)] = {x, y};
        if (id_ == 0) ++version_;               // the host's own pawn also has to re-replicate itself
        ++localSeq_;
    }

    void hostPressDoor(int door, int requester) {   // host: a door request arrived -> the host switches it
        if (id_ != 0) return;
        bool now = !door_[static_cast<size_t>(door)].open;
        door_[static_cast<size_t>(door)].open = now;
        ++version_;
        report_(0, idemDoor(version_, door), static_cast<int>(kDoorBase + static_cast<uint32_t>(door)), version_, requester, ApplyKind::Door, true, "");
    }

    void hostPickup(int item, int requester) {      // host: grant an item
        if (id_ != 0) return;
        if (item_[static_cast<size_t>(item)].holder != -1) {
            report_(0, idemPick(version_, item, requester), static_cast<int>(kItemBase + static_cast<uint32_t>(item)), version_, requester, ApplyKind::Holder, false, "already taken");
            return;
        }
        item_[static_cast<size_t>(item)].holder = requester;
        ++version_;
        report_(0, idemPick(version_, item, requester), static_cast<int>(kItemBase + static_cast<uint32_t>(item)), version_, requester, ApplyKind::Holder, true, "");
    }

    void hostSpawnZombie(int site) {                // host: wake a dormant rival in one place, one-time
        if (id_ != 0) return;
        int z = site % kEnemies;
        if (zom_[static_cast<size_t>(z)].alive) {
            report_(0, idemSpawn(zom_[static_cast<size_t>(z)].spawnVersion, site), static_cast<int>(kEnemyBase + static_cast<uint32_t>(z)), zom_[static_cast<size_t>(z)].spawnVersion, requester(site), ApplyKind::Spawn, false, "already spawned");
            return;
        }
        zom_[static_cast<size_t>(z)].alive = true;
        zom_[static_cast<size_t>(site % kEnemies)].pos = {100 + site * 17, 100 + site * 23};
        zom_[static_cast<size_t>(z)].spawnVersion = version_ + 1;
        ++version_;
        report_(0, idemSpawn(zom_[static_cast<size_t>(z)].spawnVersion, site), static_cast<int>(kEnemyBase + static_cast<uint32_t>(z)), zom_[static_cast<size_t>(z)].spawnVersion, requester(site), ApplyKind::Spawn, true, "");
    }

    void hostHitEne(int enemy, int dmg) {           // host: damage a rival
        if (id_ != 0) return;
        Zombie& z = zom_[static_cast<size_t>(enemy)];
        if (!z.alive || z.hp <= 0) { return; }      // hitting a dead one: no message to log (same as the game: it's ignored)
        z.hp -= dmg;
        if (z.hp <= 0) z.alive = false;
        ++version_;
        int kindRef = static_cast<int>(kEnemyBase + static_cast<uint32_t>(enemy));
        report_(0, idemHit(version_, enemy), kindRef, version_, id_, ApplyKind::Hp, true, "");
    }

    void hostMoveZombie(int z, int x, int y) {
        if (id_ != 0) return;
        zom_[static_cast<size_t>(z)].pos = {x, y};
        ++version_;
        report_(0, idemZombiePos(version_, z), static_cast<int>(kEnemyBase + static_cast<uint32_t>(z)), version_, 0, ApplyKind::Pos, true, "");
    }

    // -------- traffic

    void tick() {   // every 50 ms of virtual time
        if (id_ != 0) {
            if (ackedSeq_ < localSeq_) {
                Bytes b; b.push_back(kMove);
                put32(b, localSeq_);
                put32(b, static_cast<uint32_t>(pawnPos_[static_cast<size_t>(id_)].x));
                put32(b, static_cast<uint32_t>(pawnPos_[static_cast<size_t>(id_)].y));
                post(0, b);
            }
            return;
        }
        for (int c = 1; c < n_; ++c) {
            if (snapAcked_[static_cast<size_t>(c)] >= version_) continue;
            Bytes b; b.push_back(kSnap);
            put32(b, version_);
            for (int p = 0; p < n_; ++p) { put32(b, static_cast<uint32_t>(pawnPos_[static_cast<size_t>(p)].x)); put32(b, static_cast<uint32_t>(pawnPos_[static_cast<size_t>(p)].y)); }
            for (int z = 0; z < kEnemies; ++z) { put8(b, zom_[static_cast<size_t>(z)].alive ? 1u : 0u); put32(b, static_cast<uint32_t>(zom_[static_cast<size_t>(z)].pos.x)); put32(b, static_cast<uint32_t>(zom_[static_cast<size_t>(z)].pos.y)); }
            for (int d = 0; d < kDoors; ++d) put8(b, door_[static_cast<size_t>(d)].open ? 1u : 0u);
            for (int it = 0; it < kItems; ++it) put32(b, static_cast<uint32_t>(item_[static_cast<size_t>(it)].holder));
            post(c, b);
        }
    }

    void onPacket(int from, const Bytes& d) {
        if (d.size() < 5 || sum32(d.data(), d.size() - 4) != get32(d, d.size() - 4)) return;   // corrupted: ignore it, the sender retransmits
        size_t body = d.size() - 4;
        switch (d[0]) {
            case kMove:   if (id_ == 0 && body >= 13) onClientPawnMoved(from, get32(d, 1), getInt(d, 5), getInt(d, 9)); break;
            case kMoveAck:    if (id_ != 0 && body >= 5) ackedSeq_ = std::max(ackedSeq_, get32(d, 1)); break;
            case kSnap:   if (id_ != 0 && body >= 5 + static_cast<size_t>(n_) * 8 + static_cast<size_t>(kEnemies) * 9 + kDoors + kItems * 4) onSnapshot(d); break;
            case kSnapAck:    if (id_ == 0 && body >= 5) snapAcked_[static_cast<size_t>(from)] = std::max(snapAcked_[static_cast<size_t>(from)], get32(d, 1)); break;
            default:
                // requests: on a client, a "from == id_" packet is its own action; forward it to the host. The host executes it.
                if (from == id_ && id_ != 0) {
                    switch (d[0]) {
                        case kDoorReq:  if (body >= 5) { Bytes b(d); post(0, b); } break;
                        case kPickReq:  if (body >= 5) { Bytes b(d); post(0, b); } break;
                        case kSpawnReq: if (body >= 5) { Bytes b(d); post(0, b); } break;
                        case kHitReq:   if (body >= 9) { Bytes b(d); post(0, b); } break;
                        default: break;
                    }
                } else if (id_ == 0) {
                    switch (d[0]) {
                        case kDoorReq:  if (body >= 5) hostPressDoor(std::min<int>(kDoors - 1, get32(d, 1)), from); break;
                        case kPickReq:  if (body >= 5) hostPickup(std::min<int>(kItems - 1, get32(d, 1)), from); break;
                        case kSpawnReq: if (body >= 5) hostSpawnZombie(static_cast<int>(get32(d, 1))); break;
                        case kHitReq:   if (body >= 9) hostHitEne(std::min<int>(kEnemies - 1, get32(d, 1)), getInt(d, 5)); break;
                        default: break;
                    }
                }
                break;
        }
    }

private:
    static uint64_t idem(uint32_t kind, uint64_t a, uint64_t b) { return (static_cast<uint64_t>(kind) << 56) | ((a & 0xFFfull) << 40) | (b & 0xFFFFFFFFFFfull); }
    static uint64_t idemSnap(uint32_t version, uint64_t slot) { return idem(0, version, slot); }
    static uint64_t idemDoor(uint32_t version, int door) { return idem(1, version, static_cast<uint64_t>(door)); }
    static uint64_t idemPick(uint32_t version, int item, int requester) { return idem(2, version, (static_cast<uint64_t>(item) << 8) | static_cast<uint64_t>(requester)); }
    static uint64_t idemSpawn(uint32_t ver, int site) { return idem(3, ver, static_cast<uint64_t>(site)); }
    static uint64_t idemHit(uint32_t version, int ene) { return idem(4, version, static_cast<uint64_t>(ene)); }
    static uint64_t idemZombiePos(uint32_t version, int z) { return idem(5, version, static_cast<uint64_t>(z)); }
    int requester(int site) const { return 1 + (site % (n_ > 1 ? n_ - 1 : 1)); }

    void post(int to, Bytes b) {   // checksum + send callback
        put32(b, sum32(b.data(), b.size()));
        send_(to, std::move(b));
    }

    void onClientPawnMoved(int from, uint32_t seq, int x, int y) {
        uint64_t kind = 6; uint64_t idem = (kind << 56) | (static_cast<uint64_t>(from) << 32) | seq;
        if (seq <= lastSeq_[static_cast<size_t>(from)]) report_(0, idem, static_cast<int>(kPawnBase + static_cast<uint32_t>(from)), seq, from, ApplyKind::PlayerPos, false, "old sequence");
        else {
            lastSeq_[static_cast<size_t>(from)] = seq;
            pawnPos_[static_cast<size_t>(from)] = {x, y};
            ++version_;
            report_(0, idem, static_cast<int>(kPawnBase + static_cast<uint32_t>(from)), seq, from, ApplyKind::PlayerPos, true, "");
        }
        Bytes a; a.push_back(kMoveAck); put32(a, lastSeq_[static_cast<size_t>(from)]);
        post(from, a);
    }

    void onSnapshot(const Bytes& d) {
        uint32_t version = get32(d, 1);
        size_t at = 5;
        if (dedupe_ && version <= myVersion_) {
            report_(id_, idemSnap(version, 999), -1, version, 0, ApplyKind::Reset, false, "old snapshot");
        } else {
            for (int p = 0; p < n_; ++p) {
                V3 q{getInt(d, at), getInt(d, at + 4)}; at += 8;
                if (p == id_) continue;   // my pawn's position is mine: the snapshot does not overwrite it
                pawnPos_[static_cast<size_t>(p)] = q;
                report_(id_, idemSnap(version, static_cast<uint64_t>(p)), static_cast<int>(kPawnBase + static_cast<uint32_t>(p)), version, 0, ApplyKind::PlayerPos, true, "");
            }
            for (int z = 0; z < kEnemies; ++z) {
                bool alive = get8(d, at) == 1; at += 1;
                V3 q{getInt(d, at), getInt(d, at + 4)}; at += 8;
                if (zom_[static_cast<size_t>(z)].alive != alive || zom_[static_cast<size_t>(z)].pos.x != q.x || zom_[static_cast<size_t>(z)].pos.y != q.y) {
                    zom_[static_cast<size_t>(z)].alive = alive;
                    zom_[static_cast<size_t>(z)].pos = q;
                    report_(id_, idemSnap(version, 100 + static_cast<uint64_t>(z)), static_cast<int>(kEnemyBase + static_cast<uint32_t>(z)), version, 0, ApplyKind::Pos, true, "");
                } else {
                    report_(id_, idemSnap(version, 100 + static_cast<uint64_t>(z)), static_cast<int>(kEnemyBase + static_cast<uint32_t>(z)), version, 0, ApplyKind::Pos, false, "no change");
                }
            }
            for (int dc = 0; dc < kDoors; ++dc) {
                bool open = get8(d, at) == 1; at += 1;
                if (door_[static_cast<size_t>(dc)].open != open) {
                    door_[static_cast<size_t>(dc)].open = open;
                    report_(id_, idemSnap(version, 200 + static_cast<uint64_t>(dc)), static_cast<int>(kDoorBase + static_cast<uint32_t>(dc)), version, 0, ApplyKind::Door, true, "");
                } else {
                    report_(id_, idemSnap(version, 200 + static_cast<uint64_t>(dc)), static_cast<int>(kDoorBase + static_cast<uint32_t>(dc)), version, 0, ApplyKind::Door, false, "no change");
                }
            }
            for (int it = 0; it < kItems; ++it) {
                int holder = getInt(d, at); at += 4;
                int prev = item_[static_cast<size_t>(it)].holder;
                if (prev != holder) {
                    item_[static_cast<size_t>(it)].holder = holder;
                    report_(id_, idemSnap(version, 300 + static_cast<uint64_t>(it)), static_cast<int>(kItemBase + static_cast<uint32_t>(it)), version, 0, ApplyKind::Holder, true, "");
                } else {
                    report_(id_, idemSnap(version, 300 + static_cast<uint64_t>(it)), static_cast<int>(kItemBase + static_cast<uint32_t>(it)), version, 0, ApplyKind::Holder, false, "no change");
                }
            }
            myVersion_ = std::max(myVersion_, version);
        }
        Bytes a; a.push_back(kSnapAck); put32(a, myVersion_);
        post(0, a);   // ack even if we discarded the snapshot, so the host stops retransmitting
    }

    struct Zombie { bool alive = false; bool spawned = false; uint32_t spawnVersion = 0; V3 pos; int hp = 100; };
    int id_, n_;
    bool dedupe_;
    std::function<void(int, Bytes)> send_;
    ReportFn report_;
    std::vector<V3> pawnPos_;
    std::vector<Zombie> zom_{kEnemies};
    std::vector<DoorState> door_{kDoors};
    std::vector<ItemState> item_{kItems};
    uint32_t localSeq_ = 0, ackedSeq_ = 0, myVersion_ = 0, version_ = 0;
    std::vector<uint32_t> lastSeq_, snapAcked_;
};

// ====================================================================== part 2: the adapter the fuzzer drives
class Re2Target : public TargetBase {
public:
    explicit Re2Target(const Scenario& s, bool noDedupe) : net_(s.machines, s.seed * 77 + 3), n_(s.machines) {
        LinkParams link = s.link;
        if (!s.profile.empty()) linkProfile(s.profile, link);
        net_.setLink(link);
        for (int m = 0; m < n_; ++m) {
            cores_.emplace_back(new Re2Core(
                m, n_, !noDedupe, [this, m](int to, Bytes b) { net_.send(m, to, std::move(b)); },
                [this](int machine, uint64_t idem, int ref, uint32_t seq, int origin, ApplyKind kind, bool ok, const char* why) {
                    logApply(machine, idem, ref, 0, seq, origin, kind, ok, 0, why);
                }));
        }
    }

    uint64_t now() const override { return now_; }
    int machines() const override { return n_; }
    bool connected(int) const override { return true; }   // without join/leave in this MVP
    uint64_t repairs(int) const override { return 0; }    // without a path to repair (a future one: a resync for desync)
    void apply(const Op& op) override {
        int m = std::max(0, std::min(n_ - 1, op.m));
        switch (op.kind) {
            case OpKind::Move:
                ++counters_["op_move"];
                cores_[static_cast<size_t>(m)]->setLocalPawnPosition(std::max(0, std::min(1000, op.a)), std::max(0, std::min(1000, op.b)));
                break;
            case OpKind::Activate: {   // door: request from m to the host
                ++counters_["op_door"];
                cores_[static_cast<size_t>(m)]->onPacket(m, reqPacket(kDoorReq, op));
                break;
            }
            case OpKind::Pickup: {
                ++counters_["op_pickup"];
                Op p = op; p.a = op.a;   // item slot as the only field we use
                cores_[static_cast<size_t>(m)]->onPacket(m, reqPacket(kPickReq, p));
                break;
            }
            case OpKind::Spawn: {      // site = op.a
                ++counters_["op_spawn"];
                cores_[static_cast<size_t>(m)]->onPacket(m, reqPacket(kSpawnReq, op));
                break;
            }
            case OpKind::Hit: {        // victim = an alive rival, modulo the rivals
                ++counters_["op_hit"];
                int z = std::max(0, op.a) % kEnemies;
                Op p = op; p.a = z;
                cores_[static_cast<size_t>(m)]->onPacket(m, reqPacket2(kHitReq, p, op.b));
                break;
            }
            case OpKind::SetLink: {
                LinkParams lp;
                lp.minLatencyMs = static_cast<uint32_t>(std::max(0, op.a));
                lp.maxLatencyMs = static_cast<uint32_t>(std::max(std::max(0, op.a), op.b));
                lp.loss = static_cast<double>(std::max(0, op.c)) / 1000.0;
                lp.duplicate = static_cast<double>(std::max(0, op.d)) / 1000.0;
                net_.setLink(lp);
                ++counters_["op_setlink"];
                break;
            }
            default: break;   // Drop, Join, Leave, Save, Load, Stale: outside of this MVP (documented)
        }
    }

    void advance(uint64_t toMs) override {
        while (now_ < toMs) {
            now_ = std::min<uint64_t>(now_ + 10, toMs);
            net_.advance(now_);
            for (int m = 0; m < n_; ++m) { int from = 0; Bytes d; while (net_.poll(m, from, d)) cores_[static_cast<size_t>(m)]->onPacket(from, d); }
            while (now_ >= next50_) { next50_ += 50; for (auto& c : cores_) c->tick(); }
        }
    }

    void heal() override { frozen_ = true; net_.setLink(LinkParams()); }
    bool quiescent() const override {
        if (!frozen_ || !net_.idle()) return false;
        for (int mc = 1; mc < n_; ++mc) {
            const Re2Core& host = *cores_[0];
            const Re2Core& me = *cores_[static_cast<size_t>(mc)];
            if (me.myVersion() != host.hostVersion()) return false;           // my snapshot is up to date
            if (host.snapAcked(mc) != host.hostVersion()) return false;       // the host knows that mine is up to date
            if (me.moveAcked() != me.localSeq()) return false;                // my last move arrived and was acked
        }
        return true;
    }

    std::vector<Thing> view(int m) const override {
        std::vector<Thing> v;
        const Re2Core& c = *cores_[static_cast<size_t>(m)];
        for (int p = 0; p < n_; ++p) {
            Thing t; t.netId = kPawnBase + static_cast<uint32_t>(p); t.type = 0;
            t.x = c.pawnPos(p).x; t.y = c.pawnPos(p).y; t.hp = 100;
            v.push_back(t);
        }
        for (int z = 0; z < kEnemies; ++z) {
            Thing t; t.netId = kEnemyBase + static_cast<uint32_t>(z); t.type = 1;
            t.x = c.zombiePos(z).x; t.y = c.zombiePos(z).y;
            t.dead = !c.zombieAlive(z); t.hp = c.zombieAlive(z) ? 100 : 0;
            t.owner = 0;                                 // the host owns all the rivals (this MVP without transfer)
            v.push_back(t);
        }
        for (int d = 0; d < kDoors; ++d) {
            Thing t; t.netId = kDoorBase + static_cast<uint32_t>(d); t.type = 3; t.open = c.doorOpen(d);
            v.push_back(t);
        }
        for (int it = 0; it < kItems; ++it) {
            Thing t; t.netId = kItemBase + static_cast<uint32_t>(it); t.type = 2; t.holder = c.itemHolder(it);
            v.push_back(t);
        }
        std::sort(v.begin(), v.end(), [](const Thing& a, const Thing& b) { return a.netId < b.netId; });
        return v;
    }

    uint64_t digest(int m, int cat) const override {
        uint64_t h = 1469598103934665603ull + static_cast<uint64_t>(cat);
        for (auto& t : view(m)) { h ^= static_cast<uint64_t>(t.netId) * 1000003u + static_cast<uint64_t>(t.x) + static_cast<uint64_t>(t.y) * 2u + static_cast<uint64_t>(t.holder + 1) * 7919u + (t.open ? 13u : 0u) + (t.dead ? 101u : 0u); h *= 1099511628211ull; }
        return h;
    }

private:
    static Bytes reqPacket(uint8_t kind, const Op& op) {
        Bytes b; b.push_back(kind); put32(b, static_cast<uint32_t>(std::max(0, op.a)));
        return b;
    }
    static Bytes reqPacket2(uint8_t kind, const Op& op, int second) {
        Bytes b; b.push_back(kind); put32(b, static_cast<uint32_t>(std::max(0, op.a))); put32(b, static_cast<uint32_t>(std::max(0, second)));
        return b;
    }

    LossyLink net_;
    int n_;
    bool frozen_ = false;
    uint64_t now_ = 0, next50_ = 50;
    std::vector<std::unique_ptr<Re2Core>> cores_;
};

std::unique_ptr<ITarget> makeRe2(const Scenario& s, std::string* err) {
    bool noDedupe = false;
    for (auto& b : s.bugs) {
        if (b == "no_snapshot_dedupe") noDedupe = true;
        else { if (err) *err = "unknown bug class for target re2: " + b; return nullptr; }
    }
    return std::make_unique<Re2Target>(s, noDedupe);
}

const TargetRegistrar kRegisterRe2(TargetInfo{
    "re2", "RE2 remake online prototype: host-authoritative versioned snapshot (4 players, zombies, doors, items), 1 bug class", {"no_snapshot_dedupe"}, makeRe2});

}  // namespace
}  // namespace coopfuzz
