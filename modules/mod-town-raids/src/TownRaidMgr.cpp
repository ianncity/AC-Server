#include "TownRaidMgr.h"
#include "Config.h"
#include "MapMgr.h"
#include "Map.h"
#include "Creature.h"
#include "Chat.h"
#include "World.h"
#include "Random.h"
#include "MotionMaster.h"
#include "WorldSessionMgr.h"

TownRaidMgr* TownRaidMgr::instance()
{
    static TownRaidMgr instance;
    return &instance;
}

void TownRaidMgr::LoadTownDefinitions()
{
    _towns.clear();
    _towns.reserve(98);

    // 1. Stormwind City (Capital)
    _towns.push_back({
        1, "Stormwind City", 0, 1519, TownScale::Capital, 55, 65,
        Position(-8933.11f, 524.36f, 29.62f, 5.85f),
        Position(-8400.00f, 548.00f, 29.50f, 0.00f)
    });

    // 2. Orgrimmar (Capital)
    _towns.push_back({
        2, "Orgrimmar", 1, 1637, TownScale::Capital, 55, 65,
        Position(1475.25f, -4172.63f, 22.31f, 2.15f),
        Position(1604.50f, -4395.10f, 15.00f, 0.00f)
    });

    // 3. Ironforge (Capital)
    _towns.push_back({
        3, "Ironforge", 0, 1537, TownScale::Capital, 55, 65,
        Position(-4832.30f, -1069.60f, 502.30f, 4.25f),
        Position(-4880.10f, -990.50f, 502.10f, 0.00f)
    });

    // 4. Undercity (Capital)
    _towns.push_back({
        4, "Undercity", 0, 1497, TownScale::Capital, 55, 65,
        Position(1720.50f, 240.20f, 60.50f, 3.14f),
        Position(1630.10f, 240.50f, -43.10f, 0.00f)
    });

    // 5. Darnassus (Capital)
    _towns.push_back({
        5, "Darnassus", 1, 1657, TownScale::Capital, 55, 65,
        Position(9890.17f, 994.67f, 1313.92f, 5.86f),
        Position(9909.05f, 959.19f, 1315.35f, 0.00f)
    });

    // 6. Thunder Bluff (Capital)
    _towns.push_back({
        6, "Thunder Bluff", 1, 1638, TownScale::Capital, 55, 65,
        Position(-1256.15f, -130.28f, 162.97f, 4.43f),
        Position(-1209.64f, -112.70f, 163.88f, 0.00f)
    });

    // 7. Silvermoon City (Capital)
    _towns.push_back({
        7, "Silvermoon City", 530, 3487, TownScale::Capital, 55, 65,
        Position(10001.50f, -7093.24f, 47.79f, 4.47f),
        Position(9990.50f, -7052.52f, 45.45f, 0.00f)
    });

    // 8. The Exodar (Capital)
    _towns.push_back({
        8, "The Exodar", 530, 3557, TownScale::Capital, 55, 65,
        Position(-3961.19f, -11389.60f, -120.70f, 2.75f),
        Position(-3993.71f, -11413.70f, -122.14f, 0.00f)
    });

    // 9. Shattrath City (Capital)
    _towns.push_back({
        9, "Shattrath City", 530, 3703, TownScale::Capital, 65, 72,
        Position(-1908.20f, 5433.96f, -12.34f, 1.75f),
        Position(-1863.68f, 5430.04f, -9.61f, 0.00f)
    });

    // 10. Dalaran (Capital)
    _towns.push_back({
        10, "Dalaran", 571, 4395, TownScale::Capital, 75, 80,
        Position(5783.38f, 839.36f, 680.42f, 5.43f),
        Position(5797.87f, 798.38f, 661.95f, 0.00f)
    });

    // 11. Goldshire (Small)
    _towns.push_back({
        11, "Goldshire", 0, 143, TownScale::Small, 25, 30,
        Position(-9505.42f, -113.62f, 57.51f, 0.65f),
        Position(-9459.50f, 51.58f, 57.06f, 0.00f)
    });

    // 12. Sentinel Hill (Small)
    _towns.push_back({
        12, "Sentinel Hill", 0, 40, TownScale::Small, 25, 30,
        Position(-10600.10f, 1150.20f, 32.10f, 4.15f),
        Position(-10660.20f, 1000.10f, 30.50f, 0.00f)
    });

    // 13. Lakeshire (Medium)
    _towns.push_back({
        13, "Lakeshire", 0, 44, TownScale::Medium, 30, 36,
        Position(-9261.03f, -2172.66f, 64.20f, 1.27f),
        Position(-9219.37f, -2149.94f, 70.61f, 0.00f)
    });

    // 14. Darkshire (Medium)
    _towns.push_back({
        14, "Darkshire", 0, 10, TownScale::Medium, 35, 42,
        Position(-10500.20f, -1000.50f, 20.10f, 3.90f),
        Position(-10530.10f, -1190.20f, 27.60f, 0.00f)
    });

    // 15. Southshore (Medium)
    _towns.push_back({
        15, "Southshore", 0, 267, TownScale::Medium, 38, 45,
        Position(780.20f, -510.50f, 11.20f, 3.50f),
        Position(830.50f, -580.20f, 12.10f, 0.00f)
    });

    // 16. Tarren Mill (Medium)
    _towns.push_back({
        16, "Tarren Mill", 0, 267, TownScale::Medium, 38, 45,
        Position(610.10f, -390.20f, 35.40f, 4.20f),
        Position(115.20f, -450.80f, 37.10f, 0.00f)
    });

    // 17. Brill (Small)
    _towns.push_back({
        17, "Brill", 0, 85, TownScale::Small, 25, 30,
        Position(2150.20f, 245.10f, 34.20f, 0.85f),
        Position(2260.50f, 280.10f, 34.10f, 0.00f)
    });

    // 18. The Sepulcher (Small)
    _towns.push_back({
        18, "The Sepulcher", 0, 130, TownScale::Small, 25, 30,
        Position(561.54f, 1688.63f, 75.42f, 0.00f),
        Position(511.54f, 1638.63f, 75.42f, 0.00f)
    });

    // 19. Thelsamar (Small)
    _towns.push_back({
        19, "Thelsamar", 0, 38, TownScale::Small, 25, 30,
        Position(-5424.85f, -2929.87f, 347.64f, 1.64f),
        Position(-5390.18f, -2953.93f, 322.03f, 0.00f)
    });

    // 20. Menethil Harbor (Medium)
    _towns.push_back({
        20, "Menethil Harbor", 0, 11, TownScale::Medium, 35, 42,
        Position(-3790.13f, -858.47f, 11.60f, 1.12f),
        Position(-3823.06f, -834.53f, 18.28f, 0.00f)
    });

    // 21. Hammerfall (Medium)
    _towns.push_back({
        21, "Hammerfall", 0, 45, TownScale::Medium, 40, 48,
        Position(-948.30f, -3537.25f, 71.53f, 5.69f),
        Position(-907.87f, -3534.24f, 83.79f, 0.00f)
    });

    // 22. Kargath (Small)
    _towns.push_back({
        22, "Kargath", 0, 3, TownScale::Small, 45, 52,
        Position(-6681.29f, -2193.90f, 247.96f, 0.99f),
        Position(-6657.35f, -2157.10f, 264.13f, 0.00f)
    });

    // 23. Booty Bay (Medium)
    _towns.push_back({
        23, "Booty Bay", 0, 33, TownScale::Medium, 45, 52,
        Position(-14472.90f, 458.62f, 37.14f, 1.76f),
        Position(-14457.00f, 496.45f, 39.14f, 0.00f)
    });

    // 24. Grom'gol Base Camp (Medium)
    _towns.push_back({
        24, "Grom'gol Base Camp", 0, 33, TownScale::Medium, 40, 48,
        Position(-12414.00f, 166.16f, 3.48f, 2.34f),
        Position(-12432.80f, 205.17f, 2.37f, 0.00f)
    });

    // 25. Stonard (Medium)
    _towns.push_back({
        25, "Stonard", 0, 8, TownScale::Medium, 45, 52,
        Position(-10451.00f, -3290.11f, 20.30f, 0.66f),
        Position(-10487.30f, -3256.87f, 39.90f, 0.00f)
    });

    // 26. Light's Hope Chapel (Medium)
    _towns.push_back({
        26, "Light's Hope Chapel", 0, 139, TownScale::Medium, 55, 62,
        Position(2257.31f, -5340.87f, 86.80f, 1.48f),
        Position(2298.17f, -5340.98f, 90.88f, 2.18f)
    });

    // 27. Revantusk Village (Medium)
    _towns.push_back({
        27, "Revantusk Village", 0, 47, TownScale::Medium, 45, 52,
        Position(-583.73f, -4567.89f, 9.23f, 3.58f),
        Position(-622.14f, -4582.14f, 21.00f, 3.19f)
    });

    // 28. Aerie Peak (Medium)
    _towns.push_back({
        28, "Aerie Peak", 0, 47, TownScale::Medium, 45, 52,
        Position(397.88f, -2094.89f, 131.65f, 1.78f),
        Position(357.22f, -2106.09f, 121.84f, 0.00f)
    });

    // 29. Morgan's Vigil (Small)
    _towns.push_back({
        29, "Morgan's Vigil", 0, 51, TownScale::Small, 50, 58,
        Position(-8377.17f, -2780.48f, 193.69f, 1.15f),
        Position(-8365.08f, -2736.93f, 185.69f, 5.65f)
    });

    // 30. Flame Crest (Small)
    _towns.push_back({
        30, "Flame Crest", 0, 51, TownScale::Small, 50, 58,
        Position(-7459.08f, -2196.61f, 165.11f, 3.00f),
        Position(-7504.06f, -2190.77f, 165.30f, 2.18f)
    });

    // 31. Nethergarde Keep (Medium)
    _towns.push_back({
        31, "Nethergarde Keep", 0, 4, TownScale::Medium, 50, 58,
        Position(-11044.30f, -3470.16f, 65.04f, 0.94f),
        Position(-11110.20f, -3437.10f, 79.28f, 2.67f)
    });

    // 32. Kharanos (Small)
    _towns.push_back({
        32, "Kharanos", 0, 1, TownScale::Small, 20, 25,
        Position(-5651.99f, -477.61f, 396.17f, 4.73f),
        Position(-5611.80f, -475.40f, 396.90f, 0.00f)
    });

    // 33. Strahnbrad (Small)
    _towns.push_back({
        33, "Strahnbrad", 0, 267, TownScale::Small, 35, 40,
        Position(669.73f, -950.92f, 164.47f, 2.55f),
        Position(683.77f, -912.14f, 174.50f, 0.00f)
    });

    // 34. Razor Hill (Small)
    _towns.push_back({
        34, "Razor Hill", 1, 14, TownScale::Small, 25, 30,
        Position(420.50f, -4710.20f, 18.50f, 3.82f),
        Position(280.20f, -4840.10f, 10.20f, 0.00f)
    });

    // 35. Bloodhoof Village (Small)
    _towns.push_back({
        35, "Bloodhoof Village", 1, 215, TownScale::Small, 25, 30,
        Position(-2349.54f, -383.41f, -3.93f, 6.18f),
        Position(-2366.70f, -345.98f, -1.29f, 0.00f)
    });

    // 36. Dolanaar (Small)
    _towns.push_back({
        36, "Dolanaar", 1, 141, TownScale::Small, 25, 30,
        Position(9850.57f, 947.53f, 1307.84f, 0.68f),
        Position(9809.05f, 959.19f, 1315.35f, 0.00f)
    });

    // 37. The Crossroads (Medium)
    _towns.push_back({
        37, "The Crossroads", 1, 17, TownScale::Medium, 35, 42,
        Position(-400.10f, -2500.50f, 95.20f, 4.10f),
        Position(-450.20f, -2680.10f, 96.50f, 0.00f)
    });

    // 38. Camp Taurajo (Small)
    _towns.push_back({
        38, "Camp Taurajo", 1, 17, TownScale::Small, 30, 36,
        Position(-1008.67f, -3651.95f, 20.15f, 5.55f),
        Position(-1051.44f, -3653.81f, 31.14f, 0.00f)
    });

    // 39. Ratchet (Medium)
    _towns.push_back({
        39, "Ratchet", 1, 17, TownScale::Medium, 35, 42,
        Position(-2355.06f, -1955.56f, 96.28f, 0.35f),
        Position(-2372.51f, -1991.64f, 120.97f, 0.00f)
    });

    // 40. Astranaar (Medium)
    _towns.push_back({
        40, "Astranaar", 1, 331, TownScale::Medium, 35, 42,
        Position(2785.48f, -362.46f, 107.42f, 2.60f),
        Position(2756.64f, -423.06f, 119.39f, 0.00f)
    });

    // 41. Splintertree Post (Medium)
    _towns.push_back({
        41, "Splintertree Post", 1, 331, TownScale::Medium, 35, 42,
        Position(2327.36f, -2531.44f, 101.35f, 3.11f),
        Position(2343.57f, -2569.00f, 118.83f, 0.00f)
    });

    // 42. Auberdine (Medium)
    _towns.push_back({
        42, "Auberdine", 1, 148, TownScale::Medium, 30, 36,
        Position(6424.08f, 488.71f, 7.93f, 0.34f),
        Position(6410.01f, 527.03f, 14.06f, 0.00f)
    });

    // 43. Sun Rock Retreat (Small)
    _towns.push_back({
        43, "Sun Rock Retreat", 1, 406, TownScale::Small, 30, 36,
        Position(943.83f, 925.88f, 104.80f, 4.31f),
        Position(898.48f, 922.69f, 126.79f, 0.00f)
    });

    // 44. Stonetalon Peak (Small)
    _towns.push_back({
        44, "Stonetalon Peak", 1, 406, TownScale::Small, 32, 38,
        Position(2677.28f, 1507.65f, 234.95f, 4.94f),
        Position(2716.60f, 1496.82f, 236.83f, 0.00f)
    });

    // 45. Nijel's Point (Medium)
    _towns.push_back({
        45, "Nijel's Point", 1, 405, TownScale::Medium, 38, 45,
        Position(217.08f, 1283.48f, 190.38f, 1.64f),
        Position(245.59f, 1251.99f, 210.15f, 0.00f)
    });

    // 46. Shadowprey Village (Medium)
    _towns.push_back({
        46, "Shadowprey Village", 1, 405, TownScale::Medium, 38, 45,
        Position(-1625.59f, 3104.25f, 38.07f, 4.16f),
        Position(-1596.16f, 3145.26f, 61.36f, 0.00f)
    });

    // 47. Freewind Post (Medium)
    _towns.push_back({
        47, "Freewind Post", 1, 400, TownScale::Medium, 35, 42,
        Position(-5456.41f, -2419.88f, 89.59f, 5.20f),
        Position(-5477.91f, -2460.32f, 89.28f, 0.00f)
    });

    // 48. Gadgetzan (Medium)
    _towns.push_back({
        48, "Gadgetzan", 1, 440, TownScale::Medium, 48, 55,
        Position(-7050.10f, -3650.20f, 8.50f, 3.65f),
        Position(-7160.50f, -3780.10f, 9.10f, 0.00f)
    });

    // 49. Feathermoon Stronghold (Medium)
    _towns.push_back({
        49, "Feathermoon Stronghold", 1, 357, TownScale::Medium, 45, 52,
        Position(-4370.54f, 3340.11f, 12.35f, 3.42f),
        Position(-4370.83f, 3289.15f, 23.78f, 0.00f)
    });

    // 50. Camp Mojache (Medium)
    _towns.push_back({
        50, "Camp Mojache", 1, 357, TownScale::Medium, 45, 52,
        Position(-4470.09f, 297.53f, 39.42f, 2.11f),
        Position(-4461.92f, 242.58f, 45.25f, 3.56f)
    });

    // 51. Theramore Isle (Medium)
    _towns.push_back({
        51, "Theramore Isle", 1, 15, TownScale::Medium, 40, 48,
        Position(-3644.71f, -4500.49f, 9.57f, 4.42f),
        Position(-3615.49f, -4467.34f, 24.31f, 0.00f)
    });

    // 52. Brackenwall Village (Medium)
    _towns.push_back({
        52, "Brackenwall Village", 1, 15, TownScale::Medium, 40, 46,
        Position(-3149.24f, -2887.01f, 34.20f, 6.14f),
        Position(-3182.41f, -2920.82f, 33.56f, 0.00f)
    });

    // 53. Mudsprocket (Small)
    _towns.push_back({
        53, "Mudsprocket", 1, 15, TownScale::Small, 42, 48,
        Position(-4603.66f, -3210.94f, 35.01f, 1.40f),
        Position(-4622.31f, -3172.12f, 34.81f, 0.00f)
    });

    // 54. Cenarion Hold (Medium)
    _towns.push_back({
        54, "Cenarion Hold", 1, 1377, TownScale::Medium, 55, 62,
        Position(-6828.49f, 807.22f, 52.10f, 0.38f),
        Position(-6864.98f, 731.61f, 50.00f, 0.00f)
    });

    // 55. Everlook (Medium)
    _towns.push_back({
        55, "Everlook", 1, 618, TownScale::Medium, 55, 62,
        Position(6706.15f, -4633.79f, 721.87f, 5.59f),
        Position(6687.99f, -4670.07f, 723.89f, 0.00f)
    });

    // 56. Marshal's Stand (Small)
    _towns.push_back({
        56, "Marshal's Stand", 1, 490, TownScale::Small, 50, 56,
        Position(-6154.23f, -1142.67f, -216.59f, 3.00f),
        Position(-6110.54f, -1140.35f, -186.87f, 3.60f)
    });

    // 57. Honor Hold (Medium)
    _towns.push_back({
        57, "Honor Hold", 530, 3483, TownScale::Medium, 60, 66,
        Position(-665.80f, 2715.48f, 94.20f, 3.35f),
        Position(-704.89f, 2733.99f, 100.00f, 0.00f)
    });

    // 58. Thrallmar (Medium)
    _towns.push_back({
        58, "Thrallmar", 530, 3483, TownScale::Medium, 60, 66,
        Position(167.75f, 2570.89f, 79.77f, 3.46f),
        Position(192.90f, 2607.37f, 87.28f, 0.00f)
    });

    // 59. Falcon Watch (Small)
    _towns.push_back({
        59, "Falcon Watch", 530, 3483, TownScale::Small, 62, 67,
        Position(-584.37f, 4104.07f, 91.59f, 3.65f),
        Position(-574.00f, 4147.89f, 68.52f, 0.00f)
    });

    // 60. Cenarion Refuge (Small)
    _towns.push_back({
        60, "Cenarion Refuge", 530, 3521, TownScale::Small, 62, 67,
        Position(-198.97f, 5490.69f, 21.93f, 3.33f),
        Position(-172.56f, 5524.60f, 29.41f, 5.03f)
    });

    // 61. Telredor (Medium)
    _towns.push_back({
        61, "Telredor", 530, 3521, TownScale::Medium, 63, 68,
        Position(280.43f, 6046.59f, 130.39f, 4.49f),
        Position(282.22f, 6099.00f, 132.48f, 0.00f)
    });

    // 62. Zabra'jin (Medium)
    _towns.push_back({
        62, "Zabra'jin", 530, 3521, TownScale::Medium, 63, 68,
        Position(189.11f, 7964.57f, 23.82f, 2.51f),
        Position(229.35f, 7933.05f, 35.00f, 2.76f)
    });

    // 63. Garadar (Medium)
    _towns.push_back({
        63, "Garadar", 530, 3518, TownScale::Medium, 65, 70,
        Position(-1272.24f, 7083.96f, 38.44f, 2.69f),
        Position(-1224.93f, 7158.39f, 57.27f, 0.00f)
    });

    // 64. Telaar (Medium)
    _towns.push_back({
        64, "Telaar", 530, 3518, TownScale::Medium, 65, 70,
        Position(-2796.60f, 7329.24f, 42.65f, 2.62f),
        Position(-2734.68f, 7308.69f, 71.41f, 2.74f)
    });

    // 65. Allerian Stronghold (Medium)
    _towns.push_back({
        65, "Allerian Stronghold", 530, 3519, TownScale::Medium, 64, 69,
        Position(-2897.60f, 3979.81f, 25.04f, 1.57f),
        Position(-2912.37f, 4024.91f, 0.43f, 2.98f)
    });

    // 66. Stonebreaker Hold (Medium)
    _towns.push_back({
        66, "Stonebreaker Hold", 530, 3519, TownScale::Medium, 64, 69,
        Position(-2639.38f, 4425.39f, 36.77f, 2.83f),
        Position(-2620.05f, 4461.38f, 36.09f, 0.00f)
    });

    // 67. Area 52 (Medium)
    _towns.push_back({
        67, "Area 52", 530, 3523, TownScale::Medium, 67, 72,
        Position(3073.64f, 3656.38f, 143.17f, 1.41f),
        Position(3070.64f, 3699.79f, 142.48f, 2.06f)
    });

    // 68. The Stormspire (Medium)
    _towns.push_back({
        68, "The Stormspire", 530, 3523, TownScale::Medium, 68, 72,
        Position(4152.12f, 3067.70f, 336.93f, 5.78f),
        Position(4112.02f, 3071.00f, 339.46f, 0.42f)
    });

    // 69. Shadowmoon Village (Medium)
    _towns.push_back({
        69, "Shadowmoon Village", 530, 3520, TownScale::Medium, 68, 72,
        Position(-3018.00f, 2556.00f, 79.10f, 0.00f),
        Position(-2961.93f, 2559.29f, 78.52f, 0.00f)
    });

    // 70. Wildhammer Stronghold (Medium)
    _towns.push_back({
        70, "Wildhammer Stronghold", 530, 3520, TownScale::Medium, 68, 72,
        Position(-4067.65f, 2224.33f, 113.52f, 1.68f),
        Position(-4085.00f, 2176.00f, 111.68f, 3.19f)
    });

    // 71. Sylvanaar (Medium)
    _towns.push_back({
        71, "Sylvanaar", 530, 3522, TownScale::Medium, 65, 70,
        Position(2014.72f, 6881.02f, 179.02f, 4.23f),
        Position(2099.50f, 6904.67f, 183.23f, 0.82f)
    });

    // 72. Thunderlord Stronghold (Medium)
    _towns.push_back({
        72, "Thunderlord Stronghold", 530, 3522, TownScale::Medium, 65, 70,
        Position(2469.70f, 5975.94f, 154.79f, 3.02f),
        Position(2405.00f, 5941.00f, 153.00f, 0.00f)
    });

    // 73. Mok'Nathal Village (Small)
    _towns.push_back({
        73, "Mok'Nathal Village", 530, 3522, TownScale::Small, 66, 71,
        Position(2186.73f, 4729.71f, 157.04f, 1.22f),
        Position(2230.12f, 4716.43f, 161.95f, 0.00f)
    });

    // 74. Evergrove (Small)
    _towns.push_back({
        74, "Evergrove", 530, 3522, TownScale::Small, 66, 71,
        Position(2993.94f, 5473.50f, 145.20f, 2.15f),
        Position(3020.92f, 5434.32f, 146.62f, 5.39f)
    });

    // 75. Tranquillien (Small)
    _towns.push_back({
        75, "Tranquillien", 530, 3433, TownScale::Small, 25, 30,
        Position(7600.52f, -6904.99f, 93.89f, 1.78f),
        Position(7557.31f, -6889.65f, 96.03f, 0.00f)
    });

    // 76. Azure Watch (Small)
    _towns.push_back({
        76, "Azure Watch", 530, 3524, TownScale::Small, 25, 30,
        Position(-4177.66f, -12470.90f, 48.00f, 4.57f),
        Position(-4133.42f, -12471.80f, 44.57f, 0.00f)
    });

    // 77. Blood Watch (Small)
    _towns.push_back({
        77, "Blood Watch", 530, 3525, TownScale::Small, 28, 34,
        Position(-2019.67f, -11872.10f, 46.58f, 2.41f),
        Position(-2058.16f, -11895.00f, 46.15f, 0.00f)
    });

    // 78. Valiance Keep (Medium)
    _towns.push_back({
        78, "Valiance Keep", 571, 3537, TownScale::Medium, 70, 75,
        Position(2256.42f, 5178.84f, 11.85f, 1.31f),
        Position(2293.72f, 5195.29f, 12.21f, 1.15f)
    });

    // 79. Warsong Hold (Medium)
    _towns.push_back({
        79, "Warsong Hold", 571, 3537, TownScale::Medium, 70, 75,
        Position(2804.14f, 6165.57f, 85.39f, 4.82f),
        Position(2814.05f, 6205.06f, 86.01f, 0.76f)
    });

    // 80. Taunka'le Village (Small)
    _towns.push_back({
        80, "Taunka'le Village", 571, 3537, TownScale::Small, 71, 76,
        Position(3444.20f, 4128.28f, 15.58f, 5.06f),
        Position(3464.73f, 4169.05f, 17.19f, 6.21f)
    });

    // 81. Bor'gorok Outpost (Small)
    _towns.push_back({
        81, "Bor'gorok Outpost", 571, 3537, TownScale::Small, 71, 76,
        Position(4469.34f, 5729.52f, 79.28f, 2.83f),
        Position(4512.05f, 5709.59f, 81.50f, 0.00f)
    });

    // 82. Fizzcrank Airstrip (Small)
    _towns.push_back({
        82, "Fizzcrank Airstrip", 571, 3537, TownScale::Small, 71, 76,
        Position(4135.78f, 5282.23f, 25.11f, 1.19f),
        Position(4177.10f, 5280.73f, 35.69f, 6.25f)
    });

    // 83. Fort Wildervar (Small)
    _towns.push_back({
        83, "Fort Wildervar", 571, 495, TownScale::Small, 71, 76,
        Position(2477.10f, -5024.63f, 282.76f, 4.03f),
        Position(2486.59f, -5063.61f, 284.33f, 0.00f)
    });

    // 84. Westguard Keep (Medium)
    _towns.push_back({
        84, "Westguard Keep", 571, 495, TownScale::Medium, 71, 76,
        Position(1457.88f, -3318.42f, 168.88f, 5.89f),
        Position(1451.44f, -3277.97f, 180.33f, 0.86f)
    });

    // 85. Camp Winterhoof (Small)
    _towns.push_back({
        85, "Camp Winterhoof", 571, 495, TownScale::Small, 71, 76,
        Position(2633.78f, -4379.52f, 283.85f, 0.05f),
        Position(2683.27f, -4392.06f, 284.36f, 0.00f)
    });

    // 86. Kamagua (Small)
    _towns.push_back({
        86, "Kamagua", 571, 495, TownScale::Small, 71, 76,
        Position(749.83f, -2941.10f, 7.37f, 0.16f),
        Position(700.15f, -2933.40f, -3.07f, 0.00f)
    });

    // 87. Wintergarde Keep (Medium)
    _towns.push_back({
        87, "Wintergarde Keep", 571, 65, TownScale::Medium, 73, 77,
        Position(3677.80f, -729.17f, 213.38f, 1.10f),
        Position(3635.00f, -712.00f, 225.00f, 6.16f)
    });

    // 88. Venomspite (Medium)
    _towns.push_back({
        88, "Venomspite", 571, 65, TownScale::Medium, 73, 77,
        Position(3257.41f, -690.54f, 167.96f, 3.40f),
        Position(3215.04f, -677.96f, 176.00f, 5.81f)
    });

    // 89. Agmar's Hammer (Medium)
    _towns.push_back({
        89, "Agmar's Hammer", 571, 65, TownScale::Medium, 73, 77,
        Position(3842.49f, 1431.58f, 90.72f, 4.66f),
        Position(3842.40f, 1478.42f, 90.52f, 0.03f)
    });

    // 90. Stars' Rest (Small)
    _towns.push_back({
        90, "Stars' Rest", 571, 65, TownScale::Small, 73, 77,
        Position(3514.70f, 1999.16f, 64.97f, 2.29f),
        Position(3474.56f, 2009.88f, 64.87f, 0.00f)
    });

    // 91. Wyrmrest Temple (Medium)
    _towns.push_back({
        91, "Wyrmrest Temple", 571, 65, TownScale::Medium, 74, 78,
        Position(3569.93f, 300.31f, 47.40f, 4.07f),
        Position(3637.80f, 282.85f, 52.23f, 0.00f)
    });

    // 92. Amberpine Lodge (Medium)
    _towns.push_back({
        92, "Amberpine Lodge", 571, 394, TownScale::Medium, 74, 78,
        Position(3439.03f, -2753.20f, 199.47f, 6.20f),
        Position(3410.58f, -2792.51f, 201.52f, 0.62f)
    });

    // 93. Conquest Hold (Medium)
    _towns.push_back({
        93, "Conquest Hold", 571, 394, TownScale::Medium, 74, 78,
        Position(3273.75f, -2246.84f, 114.82f, 4.52f),
        Position(3275.55f, -2186.71f, 112.10f, 5.89f)
    });

    // 94. Camp Oneqwah (Small)
    _towns.push_back({
        94, "Camp Oneqwah", 571, 394, TownScale::Small, 74, 78,
        Position(3875.25f, -4501.31f, 214.43f, 3.61f),
        Position(3873.29f, -4544.74f, 209.46f, 6.13f)
    });

    // 95. Zim'Torga (Medium)
    _towns.push_back({
        95, "Zim'Torga", 571, 66, TownScale::Medium, 75, 79,
        Position(5797.79f, -3553.85f, 390.43f, 3.40f),
        Position(5765.54f, -3579.86f, 386.93f, 0.00f)
    });

    // 96. Frosthold (Medium)
    _towns.push_back({
        96, "Frosthold", 571, 67, TownScale::Medium, 77, 80,
        Position(6646.01f, -233.49f, 949.35f, 0.38f),
        Position(6679.05f, -198.67f, 951.73f, 0.24f)
    });

    // 97. K3 (Small)
    _towns.push_back({
        97, "K3", 571, 67, TownScale::Small, 77, 80,
        Position(6084.77f, -1104.16f, 418.52f, 1.10f),
        Position(6123.03f, -1090.65f, 403.30f, 0.00f)
    });

    // 98. Argent Vanguard (Medium)
    _towns.push_back({
        98, "Argent Vanguard", 571, 210, TownScale::Medium, 77, 80,
        Position(6255.77f, -17.86f, 410.13f, 0.89f),
        Position(6220.03f, 0.87f, 411.97f, 0.00f)
    });

}

void TownRaidMgr::ResetTownTimer(uint32 townId)
{
    uint32 minH = sConfigMgr->GetOption<uint32>("TownRaids.TimerMinHours", 6);
    uint32 maxH = sConfigMgr->GetOption<uint32>("TownRaids.TimerMaxHours", 18);
    if (minH > maxH)
        std::swap(minH, maxH);

    // Randomized between Min and Max hours in seconds
    uint32 seconds = urand(minH * 3600, maxH * 3600);
    _townTimers[townId] = seconds;
}

uint32 TownRaidMgr::GetTownTimer(uint32 townId) const
{
    auto it = _townTimers.find(townId);
    if (it != _townTimers.end())
        return it->second;
    return 0;
}

bool TownRaidMgr::IsRaidActive(uint32 townId) const
{
    return _activeRaids.find(townId) != _activeRaids.end();
}

std::string TownRaidMgr::GetThemeName(RaidTheme theme) const
{
    switch (theme)
    {
        case RaidTheme::Demons:         return "Burning Legion Demons";
        case RaidTheme::BanditsDefias:  return "Defias Syndicate Bandits";
        case RaidTheme::Scourge:        return "Scourge Undead";
        case RaidTheme::TwilightHammer: return "Twilight Cultists & Elementals";
        case RaidTheme::Pirates:        return "Southsea & Bloodsail Pirates";
        case RaidTheme::SilithidSwarm:  return "Silithid Hive Swarm";
        default:                        return "Unknown Invaders";
    }
}

void TownRaidMgr::Initialize()
{
    LoadTownDefinitions();

    for (TownDefinition const& town : _towns)
    {
        ResetTownTimer(town.Id);
    }
}

void TownRaidMgr::Update(uint32 diff)
{
    _checkTimer += diff;
    if (_checkTimer < 1000) // Process once per second
        return;

    uint32 elapsedSec = _checkTimer / 1000;
    _checkTimer %= 1000;

    // Map updates can report deaths concurrently; only the world thread mutates raids.
    std::vector<ObjectGuid> deaths;
    {
        std::lock_guard<std::mutex> lock(_deathMutex);
        deaths.swap(_pendingDeaths);
    }
    for (ObjectGuid const& guid : deaths)
        for (auto& pair : _activeRaids)
        {
            auto& invaders = pair.second.SpawnedInvaders;
            auto const previousSize = invaders.size();
            invaders.remove(guid);
            if (invaders.size() != previousSize)
                break;
        }

    // 1. Tick countdowns for idle towns
    for (TownDefinition const& town : _towns)
    {
        if (IsRaidActive(town.Id))
            continue;

        if (_townTimers[town.Id] <= elapsedSec)
        {
            StartRaid(town.Id);
        }
        else
        {
            _townTimers[town.Id] -= elapsedSec;
        }
    }

    // 2. Tick active raids
    std::vector<uint32> endedRaids;
    std::vector<uint32> defendedTowns;
    for (auto& pair : _activeRaids)
    {
        uint32 townId = pair.first;
        ActiveRaidInstance& raid = pair.second;

        raid.ElapsedMs += (elapsedSec * 1000);

        // Check if all spawned invaders have been slain by guards/players
        // A missing map means the raid state cannot be observed this tick; do
        // not interpret that as every invader being dead.
        bool anyAlive = true;
        TownDefinition const* townDef = nullptr;
        for (TownDefinition const& t : _towns)
        {
            if (t.Id == townId)
            {
                townDef = &t;
                break;
            }
        }

        if (townDef)
        {
            Map* map = sMapMgr->FindBaseMap(townDef->MapId);
            if (map)
            {
                anyAlive = false;
                for (auto it = raid.SpawnedInvaders.begin(); it != raid.SpawnedInvaders.end();)
                {
                    Creature* creature = map->GetCreature(*it);
                    if (!creature)
                    {
                        // Temporary summons can leave the object store when their
                        // grid unloads. Only a confirmed death removes an invader
                        // from the raid; otherwise the raid runs to its timeout.
                        anyAlive = true;
                        ++it;
                    }
                    else if (!creature->IsAlive())
                    {
                        it = raid.SpawnedInvaders.erase(it);
                    }
                    else
                    {
                        anyAlive = true;
                        // A point movement ends in IDLE. Only issue another march when
                        // the invader is still away from the target; otherwise this
                        // restarts MovePoint(1) every second after arrival, generating
                        // needless movement updates and preventing a stable arrival.
                        if (!creature->IsInCombat() &&
                            !creature->GetMotionMaster()->GetCurrentMovementGeneratorType() &&
                            !creature->IsWithinDist2d(&townDef->TownCenter, 5.0f))
                        {
                            creature->GetMotionMaster()->MovePoint(1, townDef->TownCenter);
                        }
                        ++it;
                    }
                }
            }
        }

        // If all dead, town defenders triumphed!
        if (!anyAlive)
        {
            defendedTowns.push_back(townId);
            continue;
        }

        // If raid duration exceeded, inevitable failure: invaders retreat/despawn
        if (raid.ElapsedMs >= raid.MaxDurationMs)
        {
            endedRaids.push_back(townId);
        }
    }

    for (uint32 townId : defendedTowns)
    {
        CleanupRaid(townId, true);
    }

    for (uint32 townId : endedRaids)
    {
        CleanupRaid(townId, false);
    }
}

void TownRaidMgr::OnInvaderDeath(ObjectGuid const& invaderGuid)
{
    std::lock_guard<std::mutex> lock(_deathMutex);
    _pendingDeaths.push_back(invaderGuid);
}

void TownRaidMgr::SpawnInvadersForTown(TownDefinition const& town, RaidTheme theme, ActiveRaidInstance& raid)
{
    Map* map = sMapMgr->FindBaseMap(town.MapId);
    if (!map)
        return;

    // Determine unit count based on town scale
    uint32 normalCount = 5;
    uint32 casterCount = 2;
    uint32 eliteCount = 0;
    bool hasBoss = false;

    switch (town.Scale)
    {
        case TownScale::Small:
            normalCount = urand(3, 4);
            casterCount = urand(1, 2);
            eliteCount = 1;
            hasBoss = false;
            break;
        case TownScale::Medium:
            normalCount = urand(6, 8);
            casterCount = urand(3, 4);
            eliteCount = urand(2, 3);
            hasBoss = false;
            break;
        case TownScale::Capital:
            normalCount = urand(12, 16);
            casterCount = urand(5, 7);
            eliteCount = urand(4, 6);
            hasBoss = true;
            break;
    }

    // Creature IDs per theme
    uint32 idMelee = 97001;
    uint32 idCaster = 97002;
    uint32 idElite = 97004;
    uint32 idBoss = 97006;

    switch (theme)
    {
        case RaidTheme::Demons:
            idMelee = 97001;  // Felguard Raider
            idCaster = 97002; // Felhound Stalker
            idElite = 97004;  // Wrathguard Invader
            idBoss = 97006;   // Pit Lord Destructor
            break;
        case RaidTheme::BanditsDefias:
            idMelee = 97010;  // Defias Cutpurse
            idCaster = 97012; // Defias Pyromancer
            idElite = 97013;  // Defias Enforcer
            idBoss = 97014;   // Defias Mastermind
            break;
        case RaidTheme::Scourge:
            idMelee = 97020;  // Plague Ghoul
            idCaster = 97021; // Skeletal Frostmage
            idElite = 97023;  // Plagued Monstrosity
            idBoss = 97024;   // Scourge Lich
            break;
        case RaidTheme::TwilightHammer:
            idMelee = 97030;  // Twilight Cultist
            idCaster = 97031; // Twilight Dark Ascendant
            idElite = 97032;  // Raging Fire Elemental
            idBoss = 97033;   // Twilight War-Bringer
            break;
        case RaidTheme::Pirates:
            idMelee = 97040;  // Southsea Freebooter
            idCaster = 97041; // Southsea Cannoneer
            idElite = 97043;  // Southsea First Mate
            idBoss = 97044;   // Dread Pirate Captain
            break;
        case RaidTheme::SilithidSwarm:
            idMelee = 97050;  // Silithid Swarmer
            idCaster = 97051; // Silithid Spitfire
            idElite = 97053;  // Silithid Colossus
            idBoss = 97054;   // Sandstalker Broodmother
            break;
        default:
            break;
    }

    auto SpawnUnit = [&](uint32 entry, bool isBossUnit)
    {
        // Stagger spawn slightly around road center
        float offsetX = frand(-4.0f, 4.0f);
        float offsetY = frand(-4.0f, 4.0f);
        Position spawnPos(town.RoadSpawn.GetPositionX() + offsetX,
                          town.RoadSpawn.GetPositionY() + offsetY,
                          town.RoadSpawn.GetPositionZ(),
                          town.RoadSpawn.GetOrientation());

        Creature* invader = map->SummonCreature(entry, spawnPos, nullptr, raid.MaxDurationMs);
        if (invader)
        {
            uint8 level = isBossUnit ? (town.MaxLevel + 2) : urand(town.MinLevel, town.MaxLevel);
            invader->SetLevel(level);
            invader->UpdateAllStats();
            invader->SetFaction(14); // Hostile Monster faction
            invader->SetPvP(true);

            // March directly along road towards town center
            invader->GetMotionMaster()->MovePoint(1, town.TownCenter);
            raid.SpawnedInvaders.push_back(invader->GetGUID());
        }
    };

    // Spawn frontline melee
    for (uint32 i = 0; i < normalCount; ++i)
        SpawnUnit(idMelee, false);

    // Spawn casters / ranged
    for (uint32 i = 0; i < casterCount; ++i)
        SpawnUnit(idCaster, false);

    // Spawn elites
    for (uint32 i = 0; i < eliteCount; ++i)
        SpawnUnit(idElite, false);

    // Spawn Alpha boss if capital town
    if (hasBoss)
        SpawnUnit(idBoss, true);
}

void TownRaidMgr::AnnounceRaid(TownDefinition const& town, RaidTheme theme, bool started, bool defeated)
{
    if (!sConfigMgr->GetOption<bool>("TownRaids.Announce", true))
        return;

    std::string themeName = GetThemeName(theme);
    std::string msg;

    if (started)
    {
        msg = "|cffff2020[Town Raid Alert]|r A raid party of |cffff8000" + themeName +
              "|r has been sighted on the road marching upon |cffffff00" + town.Name +
              "|r! Defend the town!";
    }
    else if (defeated)
    {
        msg = "|cff00ff00[Town Raid Defeated]|r The brave defenders of |cffffff00" + town.Name +
              "|r have repelled the invasion of |cffff8000" + themeName + "|r!";
    }
    else
    {
        msg = "|cffaaaaaa[Town Raid Concluded]|r The raid on |cffffff00" + town.Name +
              "|r has broken! The remaining invaders were driven off into the wilds.";
    }

    if (Map* map = sMapMgr->FindBaseMap(town.MapId))
        map->SendZoneText(town.ZoneId, msg.c_str());

    sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, msg);
}

bool TownRaidMgr::StartRaid(uint32 townId, int32 forceTheme)
{
    if (IsRaidActive(townId))
        return false;

    if (forceTheme < -1 || forceTheme >= static_cast<int32>(RaidTheme::MaxThemes))
        return false;

    TownDefinition const* townDef = nullptr;
    for (TownDefinition const& t : _towns)
    {
        if (t.Id == townId)
        {
            townDef = &t;
            break;
        }
    }

    if (!townDef)
        return false;

    // Ensure the overland map exists before attempting summons. A missing map or
    // missing raid creature templates must never create an active, empty raid.
    if (!sMapMgr->CreateBaseMap(townDef->MapId))
    {
        LOG_ERROR("module", "Town raid for '{}' could not start: map {} is unavailable.", townDef->Name, townDef->MapId);
        ResetTownTimer(townId);
        return false;
    }

    RaidTheme theme;
    if (forceTheme >= 0 && forceTheme < static_cast<int32>(RaidTheme::MaxThemes))
        theme = static_cast<RaidTheme>(forceTheme);
    else
    {
        // Contextual regional weighting with randomized variety
        std::string const& n = townDef->Name;
        if ((n.find("Booty Bay") != std::string::npos || n.find("Menethil") != std::string::npos ||
             n.find("Ratchet") != std::string::npos || n.find("Theramore") != std::string::npos ||
             n.find("Auberdine") != std::string::npos || n.find("Southshore") != std::string::npos) && urand(1, 100) <= 50)
        {
            theme = RaidTheme::Pirates;
        }
        else if ((n.find("Gadgetzan") != std::string::npos || n.find("Cenarion Hold") != std::string::npos ||
                  n.find("Taurajo") != std::string::npos || n.find("Freewind") != std::string::npos ||
                  n.find("Marshal") != std::string::npos) && urand(1, 100) <= 50)
        {
            theme = RaidTheme::SilithidSwarm;
        }
        else if (townDef->MapId == 571 && urand(1, 100) <= 45)
        {
            theme = RaidTheme::Scourge;
        }
        else
        {
            theme = static_cast<RaidTheme>(urand(0, static_cast<uint32>(RaidTheme::MaxThemes) - 1));
        }
    }

    uint32 durationMin = sConfigMgr->GetOption<uint32>("TownRaids.DurationMinutes", 25);
    ActiveRaidInstance raid;
    raid.TownId = townId;
    raid.Theme = theme;
    raid.ElapsedMs = 0;
    raid.MaxDurationMs = durationMin * 60 * 1000;
    raid.IsRetreating = false;

    SpawnInvadersForTown(*townDef, theme, raid);
    if (raid.SpawnedInvaders.empty())
    {
        LOG_ERROR("module", "Town raid for '{}' could not start: no invaders spawned for theme '{}'.",
                  townDef->Name, GetThemeName(theme));
        ResetTownTimer(townId);
        return false;
    }

    _activeRaids[townId] = raid;

    AnnounceRaid(*townDef, theme, true, false);
    return true;
}

void TownRaidMgr::CleanupRaid(uint32 townId, bool defeatedByDefenders)
{
    auto it = _activeRaids.find(townId);
    if (it == _activeRaids.end())
        return;

    ActiveRaidInstance& raid = it->second;

    TownDefinition const* townDef = nullptr;
    for (TownDefinition const& t : _towns)
    {
        if (t.Id == townId)
        {
            townDef = &t;
            break;
        }
    }

    if (townDef)
    {
        Map* map = sMapMgr->FindBaseMap(townDef->MapId);
        if (map)
        {
            for (ObjectGuid const& guid : raid.SpawnedInvaders)
            {
                if (Creature* creature = map->GetCreature(guid))
                {
                    creature->DespawnOrUnsummon(Milliseconds(2000)); // Clean 2-second despawn
                }
            }
        }

        AnnounceRaid(*townDef, raid.Theme, false, defeatedByDefenders);
    }

    _activeRaids.erase(it);
    ResetTownTimer(townId);
}

bool TownRaidMgr::StopRaid(uint32 townId)
{
    if (!IsRaidActive(townId))
        return false;

    CleanupRaid(townId, false);
    return true;
}
