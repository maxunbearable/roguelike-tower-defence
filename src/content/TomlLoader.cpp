#include "content/TomlLoader.h"

#include <sstream>
#include <stdexcept>
#include <string>

#include <toml++/toml.hpp>

#include "content/WaveGen.h"

namespace td::content {
namespace {

[[noreturn]] void fail(const std::filesystem::path& file, const std::string& what) {
    throw std::runtime_error(file.string() + ": " + what);
}

template <typename T>
T require(const toml::node_view<const toml::node>& node, const std::filesystem::path& file,
          const std::string& key) {
    const auto v = node[key].value<T>();
    if (!v) fail(file, "missing or wrong-typed key '" + key + "'");
    return *v;
}

float requireFloat(const toml::node_view<const toml::node>& node,
                   const std::filesystem::path& file, const std::string& key) {
    // TOML distinguishes 1 from 1.0; accepting both stops "speed = 2" from
    // being a confusing load failure in a hand-written content file.
    if (auto d = node[key].value<double>()) return static_cast<float>(*d);
    if (auto i = node[key].value<int64_t>()) return static_cast<float>(*i);
    fail(file, "missing or wrong-typed key '" + key + "'");
}

float optFloat(const toml::node_view<const toml::node>& node, const std::string& key,
               float fallback) {
    if (auto d = node[key].value<double>()) return static_cast<float>(*d);
    if (auto i = node[key].value<int64_t>()) return static_cast<float>(*i);
    return fallback;
}

int optInt(const toml::node_view<const toml::node>& node, const std::string& key, int fallback) {
    if (auto i = node[key].value<int64_t>()) return static_cast<int>(*i);
    return fallback;
}

std::vector<std::string> splitTileRows(const std::string& block) {
    std::vector<std::string> rows;
    std::istringstream ss(block);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) rows.push_back(line);
    }
    return rows;
}

}  // namespace

std::vector<EnemyDef> loadEnemies(const std::filesystem::path& file) {
    const toml::table tbl = toml::parse_file(file.string());
    const auto* arr = tbl["enemy"].as_array();
    if (!arr) fail(file, "expected an [[enemy]] array");

    std::vector<EnemyDef> out;
    for (const auto& elem : *arr) {
        const auto* t = elem.as_table();
        if (!t) fail(file, "[[enemy]] entry is not a table");
        const auto n = toml::node_view<const toml::node>(t);
        EnemyDef e;
        e.id = require<std::string>(n, file, "id");
        e.name = require<std::string>(n, file, "name");
        e.maxHp = requireFloat(n, file, "maxHp");
        e.armor = requireFloat(n, file, "armor");
        e.speed = requireFloat(n, file, "speed");
        e.bounty = static_cast<int>(require<int64_t>(n, file, "bounty"));
        e.shardValue = static_cast<int>(require<int64_t>(n, file, "shardValue"));
        e.flying = require<bool>(n, file, "flying");
        e.boss = n["boss"].value<bool>().value_or(false);
        e.sprite = n["sprite"].value<std::string>().value_or(std::string{});
        e.spriteScale = std::max(1, static_cast<int>(n["spriteScale"].value<int64_t>().value_or(1)));
        if (const auto* tint = t->get("tint") ? t->get("tint")->as_array() : nullptr) {
            if (tint->size() == 3) {
                const auto ch = [&](size_t i) {
                    return static_cast<int>(tint->get(i)->value<int64_t>().value_or(255));
                };
                e.tintR = ch(0);
                e.tintG = ch(1);
                e.tintB = ch(2);
            }
        }
        if (const auto* rt = t->get("resist") ? t->get("resist")->as_table() : nullptr) {
            for (const auto& [k, v] : *rt) {
                if (auto f = v.value<double>()) e.resist[std::string(k.str())] = static_cast<float>(*f);
                else if (auto i = v.value<int64_t>())
                    e.resist[std::string(k.str())] = static_cast<float>(*i);
            }
        }
        out.push_back(std::move(e));
    }
    return out;
}

TowerDef loadTower(const std::filesystem::path& file) {
    const toml::table tbl = toml::parse_file(file.string());
    const auto* t = tbl["tower"].as_table();
    if (!t) fail(file, "expected a [tower] table");
    const auto n = toml::node_view<const toml::node>(t);

    TowerDef d;
    d.id = require<std::string>(n, file, "id");
    d.name = require<std::string>(n, file, "name");
    d.desc = n["desc"].value<std::string>().value_or(std::string{});
    d.buildCost = static_cast<int>(require<int64_t>(n, file, "buildCost"));
    d.sellRefundPct = requireFloat(n, file, "sellRefundPct");
    d.damage = requireFloat(n, file, "damage");
    d.fireRate = requireFloat(n, file, "fireRate");
    d.range = requireFloat(n, file, "range");
    d.projectileSpeed = requireFloat(n, file, "projectileSpeed");
    d.projectileCount = static_cast<int>(require<int64_t>(n, file, "projectileCount"));
    d.pierce = static_cast<int>(require<int64_t>(n, file, "pierce"));
    d.critChance = requireFloat(n, file, "critChance");
    d.critMult = requireFloat(n, file, "critMult");
    d.armorPen = requireFloat(n, file, "armorPen");
    d.targetPriority = require<std::string>(n, file, "targetPriority");
    d.damageType = n["damageType"].value_or<std::string>("physical");
    d.specCost = optInt(n, "specCost", 0);

    if (const auto* levels = tbl["level"].as_array()) {
        for (const auto& elem : *levels) {
            const auto* lt = elem.as_table();
            if (!lt) fail(file, "[[level]] entry is not a table");
            const auto ln = toml::node_view<const toml::node>(lt);
            TowerLevel lv;
            lv.cost = static_cast<int>(require<int64_t>(ln, file, "cost"));
            lv.damageMult = requireFloat(ln, file, "damageMult");
            lv.rangeMult = requireFloat(ln, file, "rangeMult");
            lv.fireRateMult = requireFloat(ln, file, "fireRateMult");
            d.levels.push_back(lv);
        }
    }
    return d;
}

MapDef loadMap(const std::filesystem::path& file) {
    const toml::table tbl = toml::parse_file(file.string());
    const auto* m = tbl["map"].as_table();
    if (!m) fail(file, "expected a [map] table");
    const auto n = toml::node_view<const toml::node>(m);

    MapDef d;
    d.id = require<std::string>(n, file, "id");
    d.name = require<std::string>(n, file, "name");
    // Optional: campaign position and the one-line description shown on the map
    // select card. A map without an order falls to the end.
    d.order = static_cast<int>(n["order"].value<int64_t>().value_or(0));
    d.blurb = n["blurb"].value<std::string>().value_or(std::string{});
    d.gridW = static_cast<int>(require<int64_t>(n, file, "gridW"));
    d.gridH = static_cast<int>(require<int64_t>(n, file, "gridH"));
    d.startGold = static_cast<int>(require<int64_t>(n, file, "startGold"));
    d.buildTime = requireFloat(n, file, "buildTime");
    d.tileRows = splitTileRows(require<std::string>(n, file, "tiles"));

    const auto* path = m->get("path") ? m->get("path")->as_array() : nullptr;
    if (!path) fail(file, "missing 'path' array");
    for (const auto& wp : *path) {
        const auto* pair = wp.as_array();
        if (!pair || pair->size() != 2) fail(file, "path waypoint must be a [x, y] pair");
        const auto x = (*pair)[0].value<int64_t>();
        const auto y = (*pair)[1].value<int64_t>();
        if (!x || !y) fail(file, "path waypoint coordinates must be integers");
        d.pathWaypoints.push_back(core::Vec2{static_cast<float>(*x), static_cast<float>(*y)});
    }

    // A [waves] recipe expands into concrete waves; an explicit [[wave]] array
    // is still honoured, which keeps small hand-authored test maps possible.
    if (const auto* rec = tbl["waves"].as_table()) {
        const auto rn = toml::node_view<const toml::node>(rec);
        WaveRecipe wr;
        wr.count = optInt(rn, "count", 0);
        wr.countBase = optInt(rn, "countBase", wr.countBase);
        wr.countPerWave = optFloat(rn, "countPerWave", wr.countPerWave);
        wr.intervalBase = optFloat(rn, "intervalBase", wr.intervalBase);
        wr.intervalDecay = optFloat(rn, "intervalDecay", wr.intervalDecay);
        wr.intervalMin = optFloat(rn, "intervalMin", wr.intervalMin);
        wr.hpPerWave = optFloat(rn, "hpPerWave", wr.hpPerWave);
        wr.hpCurveExp = optFloat(rn, "hpCurveExp", wr.hpCurveExp);
        wr.armorPerWave = optFloat(rn, "armorPerWave", wr.armorPerWave);
        wr.bountyPerWave = optFloat(rn, "bountyPerWave", wr.bountyPerWave);
        wr.secondaryFromWave = optInt(rn, "secondaryFromWave", wr.secondaryFromWave);
        wr.secondaryFraction = optFloat(rn, "secondaryFraction", wr.secondaryFraction);
        wr.secondaryDelay = optFloat(rn, "secondaryDelay", wr.secondaryDelay);
        wr.delay = optFloat(rn, "delay", wr.delay);
        wr.bossDelay = optFloat(rn, "bossDelay", wr.bossDelay);

        const auto* pool = rec->get("pool") ? rec->get("pool")->as_array() : nullptr;
        if (!pool) fail(file, "[waves] recipe has no [[waves.pool]] entries");
        for (const auto& pelem : *pool) {
            const auto* pt = pelem.as_table();
            if (!pt) fail(file, "[[waves.pool]] entry is not a table");
            const auto pn = toml::node_view<const toml::node>(pt);
            WavePoolEntry entry;
            entry.enemyId = require<std::string>(pn, file, "enemy");
            entry.fromWave = optInt(pn, "fromWave", 1);
            wr.pool.push_back(std::move(entry));
        }
        // [[waves.boss]] is optional: a map without one simply has no boss.
        if (const auto* bosses = rec->get("boss") ? rec->get("boss")->as_array() : nullptr) {
            for (const auto& belem : *bosses) {
                const auto* bt = belem.as_table();
                if (!bt) fail(file, "[[waves.boss]] entry is not a table");
                const auto bn = toml::node_view<const toml::node>(bt);
                BossWave bw;
                bw.wave = static_cast<int>(require<int64_t>(bn, file, "wave"));
                bw.enemyId = require<std::string>(bn, file, "enemy");
                wr.bosses.push_back(std::move(bw));
            }
        }
        if (wr.count <= 0) fail(file, "[waves] recipe needs a positive count");
        d.hasRecipe = true;
        d.recipe = wr;
        d.waves = generateWaves(wr);
    } else if (const auto* waves = tbl["wave"].as_array()) {
        for (const auto& elem : *waves) {
            const auto* wt = elem.as_table();
            if (!wt) fail(file, "[[wave]] entry is not a table");
            const auto wn = toml::node_view<const toml::node>(wt);
            WaveDef w;
            w.delay = requireFloat(wn, file, "delay");
            const auto* groups = wt->get("group") ? wt->get("group")->as_array() : nullptr;
            if (!groups) fail(file, "wave has no [[wave.group]] entries");
            for (const auto& gelem : *groups) {
                const auto* gt = gelem.as_table();
                if (!gt) fail(file, "[[wave.group]] entry is not a table");
                const auto gn = toml::node_view<const toml::node>(gt);
                WaveGroup g;
                g.enemyId = require<std::string>(gn, file, "enemy");
                g.count = static_cast<int>(require<int64_t>(gn, file, "count"));
                g.interval = requireFloat(gn, file, "interval");
                g.startDelay = requireFloat(gn, file, "startDelay");
                w.groups.push_back(std::move(g));
            }
            d.waves.push_back(std::move(w));
        }
    }
    return d;
}

}  // namespace td::content
