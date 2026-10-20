// Descompilado de /home/ruben/Escritorio/DCB/Ruckman/ruckman/ruckman.dcb
// origen: Fenix 0x0200   destino: Fenix
// dcb2prg fab00c490b  (decompile.py de 2026-09-01 15:34)
// 38 procesos, 0 identificadores, 95 cadenas
// Ficheros fuente originales: P2X Stuff\fenixDevKit\fenix\pacman\editor\ruckman11.prg

// 411 globales del motor omitidas (las declara Fenix)

GLOBAL
    INT unresolved_global_1680;
    INT unresolved_global_1684;
    INT unresolved_global_1688;
    INT unresolved_global_1692;
    INT unresolved_global_1696[4];
    INT unresolved_global_1700;
    INT unresolved_global_1704;
    INT unresolved_global_1708;
    INT unresolved_global_1712;
    INT unresolved_global_1724;
    INT unresolved_global_1728;
    INT unresolved_global_1732;
    INT unresolved_global_1736;
    INT unresolved_global_1740;
    INT unresolved_global_1744;
    INT unresolved_global_1748;
    INT unresolved_global_1752;
    INT unresolved_global_1756;
    INT unresolved_global_1760;
    INT unresolved_global_1764;
    INT unresolved_global_1768;
    INT unresolved_global_1772;
    INT unresolved_global_1776;
    INT unresolved_global_1780;
    INT unresolved_global_1784;
    INT unresolved_global_1788;
    INT unresolved_global_1792;
    INT unresolved_global_1796;
    INT unresolved_global_1800;
    INT unresolved_global_1804;
    INT unresolved_global_1808;
    INT unresolved_global_1812;
    INT unresolved_global_1816;
    INT unresolved_global_1820;
    INT unresolved_global_1824;
    INT unresolved_global_1828;
    INT unresolved_global_1832;
    INT unresolved_global_1836;
    INT unresolved_global_1840;
    INT unresolved_global_1844;
    INT unresolved_global_1848;
    INT unresolved_global_1852;
    INT unresolved_global_1856;
    INT unresolved_global_1860;
    INT unresolved_global_1864;
    INT unresolved_global_1868;
    INT unresolved_global_1872;
    INT unresolved_global_1876;
    INT unresolved_global_1880;
    INT unresolved_global_1884;
    INT unresolved_global_1888[50][21][14];
    INT unresolved_global_69208[21][14];
    INT unresolved_global_70528;
    INT unresolved_global_70532;
    INT unresolved_global_70580;
    INT unresolved_global_70584;
    INT unresolved_global_70588;
    INT unresolved_global_70592;
    INT unresolved_global_70596;
    INT unresolved_global_70600;
    INT unresolved_global_70604;
    INT unresolved_global_70608;
    INT unresolved_global_70612;
    INT unresolved_global_70616;
    INT unresolved_global_70620;
    INT unresolved_global_70624;
    INT unresolved_global_70628;
    INT unresolved_global_70632;
    INT unresolved_global_70636;
    INT unresolved_global_70640;
    INT unresolved_global_70644;
    INT unresolved_global_70648;
    INT unresolved_global_70652;
    INT unresolved_global_70656;
    INT unresolved_global_70660;
    INT unresolved_global_70664;
    INT unresolved_global_70668;
    INT unresolved_global_70672;
    INT unresolved_global_70706;
    INT unresolved_global_70740;
    INT unresolved_global_70944;
    INT unresolved_global_70978;

END

// Valores iniciales de las globales, tal como los guarda el
// .dcb. MAIN llama a esto antes que a nada.
FUNCTION fenix_init_globals()
BEGIN
    unresolved_global_70672 = 31855121;
    unresolved_global_70706 = -1995898607;
    unresolved_global_70740 = 50345985;
    unresolved_global_70944 = -1995898607;
    unresolved_global_70978 = 50345985;
END

BEGIN
    unresolved_global_70580 = load_wav("chomp.wav");
    unresolved_global_70584 = load_wav("drop.wav");
    unresolved_global_70588 = load_wav("tik_tok.wav");
    unresolved_global_70592 = load_wav("explosion.wav");
    unresolved_global_70596 = load_wav("money.wav");
    unresolved_global_70600 = load_wav("eat_fruit.wav");
    unresolved_global_70604 = load_wav("eat_ghost1.wav");
    unresolved_global_70608 = load_wav("eat_ghost2.wav");
    unresolved_global_70612 = load_wav("eat_ghost3.wav");
    unresolved_global_70616 = load_wav("eat_ghost4.wav");
    unresolved_global_70620 = load_wav("ghost.wav");
    unresolved_global_70624 = load_wav("freeze.wav");
    unresolved_global_70628 = load_wav("rainbow.wav");
    unresolved_global_70632 = load_wav("ice_smash.wav");
    unresolved_global_70636 = load_wav("power_pellet.wav");
    unresolved_global_70640 = load_wav("eat_pellet.wav");
    unresolved_global_70644 = load_wav("hit.wav");
    unresolved_global_70648 = load_wav("regenerator.wav");
    unresolved_global_70652 = load_wav("lose1.wav");
    unresolved_global_70656 = load_wav("lose2.wav");
    unresolved_global_70660 = load_wav("move.wav");
    unresolved_global_70664 = load_wav("eat1.wav");
    unresolved_global_70668 = load_wav("eat2.wav");
    unresolved_global_1836 = load_song("song1.ogg");
    unresolved_global_1840 = load_song("song2.ogg");
    unresolved_global_1844 = load_song("song3.ogg");
    unresolved_global_1848 = load_song("song4.ogg");
    unresolved_global_1856 = load_song("welldone.ogg");
    unresolved_global_1852 = load_song("death.ogg");
    unresolved_global_1860 = load_song("titleloop.ogg");
    unresolved_global_1820 = load_fnt("med.fnt");
    unresolved_global_1824 = load_fnt("grey.fnt");
    unresolved_global_1828 = load_fnt("small.fnt");
    unresolved_global_1784 = -1;
    unresolved_global_1796 = -1;
    unresolved_global_1808 = 1;
    unresolved_global_1776 = 20;
    unresolved_global_1868 = 1;
    unresolved_global_1864 = 1;
    unresolved_global_1688 = 2;
    unresolved_global_1788 = 0;
    unresolved_global_1792 = 0;
    IF (file_exists("levels"))
        load("levels", UNRESOLVED_GLOBAL_1888);
    END
    IF (file_exists("hiscores"))
        load("hiscores", UNRESOLVED_GLOBAL_1696);
    END
    IF (file_exists("options"))
        load("options", UNRESOLVED_GLOBAL_1864);
    ELSE
        unresolved_global_1864 = 0;
        unresolved_global_1868 = 0;
        unresolved_global_1872 = 100;
        unresolved_global_1876 = 100;
        unresolved_global_1880 = 3;
    END
    set_mode(320, 240, 16);
    set_fps(30, 0);
    unresolved_global_70528 = load_fpg("pac_tiles.fpg");
    unresolved_global_70532 = load_fpg("pac.fpg");
    define_region(1, 0, 16, 320, 224);
    proc_687();
    LOOP
        FRAME;
        IF (key(28) && key(57))
            exit();
        END
    END
END

PROCESS proc_687()
BEGIN
    IF (is_playing_song() == 0)
        play_song(unresolved_global_1860, -1);
    END
    set_wav_volume(-1, unresolved_global_1872);
    set_song_volume(unresolved_global_1876);
    timer[5] = 0;
    file = unresolved_global_70532;
    graph = 500;
    x = 160;
    y = 120;
    set_text_color(rgb(255, 255, 255));
    FOR (alpha = 0; alpha < 256; alpha = alpha + 16)
        FRAME;
    END
    graph = 536;
    put_screen(unresolved_global_70532, 500);
    x = 200;
    y = 185;
    REPEAT
        IF (key(72) == 0 && (key(80) == 0 && key(56) == 0))
            unresolved_global_1800 = 0;
        END
        IF (key(72) && unresolved_global_1800 == 0)
            graph = graph - 1;
            unresolved_global_1800 = 1;
            timer[5] = 0;
            play_wav(unresolved_global_70660, 0);
        END
        IF (key(80) && unresolved_global_1800 == 0)
            graph = graph + 1;
            unresolved_global_1800 = 1;
            timer[5] = 0;
            play_wav(unresolved_global_70660, 0);
        END
        IF (graph > 539)
            graph = 536;
        END
        IF (graph < 536)
            graph = 539;
        END
        FRAME;
        IF (timer[5] > 1000)
            graph = 0;
            BREAK;
        END
    UNTIL (key(56) && unresolved_global_1800 == 0);
    IF (graph != 0)
        play_wav(unresolved_global_70640, 0);
    END
    unresolved_global_1800 = 1;
    IF (graph == 536)
        proc_773(0);
    END
    IF (graph == 537)
        proc_774();
    END
    IF (graph == 538)
        proc_775();
    END
    IF (graph == 539)
        proc_776(1);
    END
    IF (graph == 0)
        proc_776(0);
    END
END

FUNCTION proc_688()
BEGIN
    unresolved_global_1776 = unresolved_global_1808 + 19;
    IF (unresolved_global_1864 == 1)
        unresolved_global_1796 = rand(0, 9);
    END
    IF (unresolved_global_1864 == 0)
        unresolved_global_1796++;
    END
    IF (unresolved_global_1816 != 3)
        IF (unresolved_global_1868 == 1 && unresolved_global_1816 != 3)
            unresolved_global_1784 = rand(0, 19);
        END
        IF (unresolved_global_1868 == 0)
            unresolved_global_1784++;
        END
    END
    IF (unresolved_global_1784 > 19)
        unresolved_global_1784 = 0;
    END
    IF (unresolved_global_1796 > 9)
        unresolved_global_1796 = 0;
    END
    unresolved_global_1832++;
    IF (unresolved_global_1832 > 4)
        unresolved_global_1832 = 1;
    END
    IF (unresolved_global_1832 == 1)
        play_song(unresolved_global_1836, -1);
    END
    IF (unresolved_global_1832 == 2)
        play_song(unresolved_global_1840, -1);
    END
    IF (unresolved_global_1832 == 3)
        play_song(unresolved_global_1844, -1);
    END
    IF (unresolved_global_1832 == 4)
        play_song(unresolved_global_1848, -1);
    END
    proc_689();
END

PROCESS proc_689()
PRIVATE
    INT unresolved_private_3_0;  // offset 0
    INT unresolved_private_3_4;  // offset 4
    INT unresolved_private_3_8;  // offset 8
BEGIN
    IF (unresolved_global_1784 < 0)
        unresolved_global_1784 = 49;
    END
    IF (unresolved_global_1784 > 49)
        unresolved_global_1784 = 0;
    END
    IF (unresolved_global_1796 > 9)
        unresolved_global_1796 = 0;
    END
    IF (unresolved_global_1796 < 0)
        unresolved_global_1796 = 9;
    END
    file = unresolved_global_70528;
    graph = 1;
    FOR (unresolved_private_3_0 = 0; unresolved_private_3_0 < 21; unresolved_private_3_0++)
        FOR (unresolved_private_3_4 = 0; unresolved_private_3_4 < 15; unresolved_private_3_4++)
            put(unresolved_global_70528, 17 + unresolved_global_1796 * 20, unresolved_private_3_0 * 16, unresolved_private_3_4 * 16 + 16);
        END
    END
    FOR (unresolved_private_3_0 = 0; unresolved_private_3_0 < 21; unresolved_private_3_0++)
        FOR (unresolved_private_3_4 = 0; unresolved_private_3_4 < 15; unresolved_private_3_4++)
            IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4] == 1)
                unresolved_private_3_8 = 401;
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0 - 1][unresolved_private_3_4] == 1 && unresolved_private_3_0 > 0)
                    unresolved_private_3_8 = unresolved_private_3_8 + 1;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4 - 1] == 1 && unresolved_private_3_4 > 0)
                    unresolved_private_3_8 = unresolved_private_3_8 + 2;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0 + 1][unresolved_private_3_4] == 1 && unresolved_private_3_0 < 20)
                    unresolved_private_3_8 = unresolved_private_3_8 + 4;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4 + 1] == 1 && unresolved_private_3_4 < 14)
                    unresolved_private_3_8 = unresolved_private_3_8 + 8;
                END
                xput(unresolved_global_70528, unresolved_private_3_8, unresolved_private_3_0 * 16 + 8, unresolved_private_3_4 * 16 + 24, 0, 100, 4, 0);
            END
            IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4] == 2)
                unresolved_private_3_8 = 401;
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0 - 1][unresolved_private_3_4] == 2 && unresolved_private_3_0 > 0)
                    unresolved_private_3_8 = unresolved_private_3_8 + 1;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4 - 1] == 2 && unresolved_private_3_4 > 0)
                    unresolved_private_3_8 = unresolved_private_3_8 + 2;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0 + 1][unresolved_private_3_4] == 2 && unresolved_private_3_0 < 20)
                    unresolved_private_3_8 = unresolved_private_3_8 + 4;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4 + 1] == 2 && unresolved_private_3_4 < 14)
                    unresolved_private_3_8 = unresolved_private_3_8 + 8;
                END
                xput(unresolved_global_70528, unresolved_private_3_8, unresolved_private_3_0 * 16 + 8, unresolved_private_3_4 * 16 + 24, 0, 100, 4, 0);
            END
            IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4] == 4)
                xput(unresolved_global_70528, 401, unresolved_private_3_0 * 16 + 8, unresolved_private_3_4 * 16 + 24, 0, 100, 4, 0);
            END
        END
    END
    FOR (unresolved_private_3_0 = 1; unresolved_private_3_0 < 20; unresolved_private_3_0++)
        IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][0] == 0)
            xput(unresolved_global_70528, 417, unresolved_private_3_0 * 16, 32, 90000, 100, 4, 0);
        END
        IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][14] == 0)
            xput(unresolved_global_70528, 417, unresolved_private_3_0 * 16, 224, -90000, 100, 4, 0);
        END
    END
    FOR (unresolved_private_3_4 = 1; unresolved_private_3_4 < 14; unresolved_private_3_4++)
        IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_4][0] == 0)
            xput(unresolved_global_70528, 417, 16, unresolved_private_3_4 * 16 + 16, 180000, 100, 4, 0);
        END
        IF (unresolved_global_1888[unresolved_global_1784][20][unresolved_private_3_4] == 0)
            xput(unresolved_global_70528, 417, 304, unresolved_private_3_4 * 16 + 16, 0, 100, 4, 0);
        END
    END
    FOR (unresolved_private_3_0 = 0; unresolved_private_3_0 < 21; unresolved_private_3_0++)
        FOR (unresolved_private_3_4 = 0; unresolved_private_3_4 < 15; unresolved_private_3_4++)
            IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4] == 1)
                unresolved_private_3_8 = 1 + unresolved_global_1796 * 20;
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0 - 1][unresolved_private_3_4] == 1 && unresolved_private_3_0 > 0)
                    unresolved_private_3_8 = unresolved_private_3_8 + 1;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4 - 1] == 1 && unresolved_private_3_4 > 0)
                    unresolved_private_3_8 = unresolved_private_3_8 + 2;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0 + 1][unresolved_private_3_4] == 1 && unresolved_private_3_0 < 20)
                    unresolved_private_3_8 = unresolved_private_3_8 + 4;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4 + 1] == 1 && unresolved_private_3_4 < 14)
                    unresolved_private_3_8 = unresolved_private_3_8 + 8;
                END
                put(unresolved_global_70528, unresolved_private_3_8, unresolved_private_3_0 * 16, unresolved_private_3_4 * 16 + 16);
            END
            IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4] == 2)
                unresolved_private_3_8 = 1 + unresolved_global_1796 * 20;
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0 - 1][unresolved_private_3_4] == 2 && unresolved_private_3_0 > 0)
                    unresolved_private_3_8 = unresolved_private_3_8 + 1;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4 - 1] == 2 && unresolved_private_3_4 > 0)
                    unresolved_private_3_8 = unresolved_private_3_8 + 2;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0 + 1][unresolved_private_3_4] == 2 && unresolved_private_3_0 < 20)
                    unresolved_private_3_8 = unresolved_private_3_8 + 4;
                END
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4 + 1] == 2 && unresolved_private_3_4 < 14)
                    unresolved_private_3_8 = unresolved_private_3_8 + 8;
                END
                put(unresolved_global_70528, unresolved_private_3_8, unresolved_private_3_0 * 16, unresolved_private_3_4 * 16 + 16);
            END
            IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4] == 4)
                xput(unresolved_global_70528, 1 + unresolved_global_1796 * 20, unresolved_private_3_0 * 16, unresolved_private_3_4 * 16 + 16, 0, 100, 4, 0);
                xput(unresolved_global_70528, 1 + unresolved_global_1796 * 20, unresolved_private_3_0 * 16, unresolved_private_3_4 * 16 + 16, 0, 100, 4, 0);
            END
            IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4] == -1)
                unresolved_global_1736 = unresolved_private_3_0 * 160;
                unresolved_global_1740 = unresolved_private_3_4 * 160 + 160;
            END
            IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4] == 5)
                unresolved_global_1768 = unresolved_private_3_0 * 160;
                unresolved_global_1772 = unresolved_private_3_4 * 160 + 160;
            END
            IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_3_0][unresolved_private_3_4] == -2)
                unresolved_global_1728 = unresolved_private_3_0 * 160;
                unresolved_global_1732 = unresolved_private_3_4 * 160 + 160;
            END
        END
    END
    put(unresolved_global_70532, 503, 160, 8);
    FOR (x = 1; x < 20; x = x + 1)
        FOR (y = 1; y < 14; y = y + 1)
            unresolved_global_69208[x][y] = 0;
        END
    END
    FOR (x = 1; x < 20; x = x + 1)
        FOR (y = 1; y < 14; y = y + 1)
            IF (unresolved_global_1816 == 1 || unresolved_global_1816 == 4)
                IF (unresolved_global_1888[unresolved_global_1784][x][y] == 0)
                    unresolved_global_69208[x][y] = 1;
                END
            END
            IF (unresolved_global_1816 != 3)
                IF (unresolved_global_1888[unresolved_global_1784][x][y] == -2)
                    unresolved_global_69208[x][y] = 2;
                    IF (unresolved_global_1816 == 2)
                        proc_693(x, y);
                    END
                END
            END
        END
    END
    unresolved_global_69208[unresolved_global_1768 / 160 + 1][(unresolved_global_1772 - 160) / 160] = 0;
    unresolved_global_69208[unresolved_global_1768 / 160][(unresolved_global_1772 - 160) / 160 + 1] = 0;
    unresolved_global_69208[unresolved_global_1768 / 160 - 1][(unresolved_global_1772 - 160) / 160] = 0;
    unresolved_global_69208[unresolved_global_1768 / 160][(unresolved_global_1772 - 160) / 160 - 1] = 0;
    FRAME;
    unresolved_global_1680 = 1;
    unresolved_global_1684 = 1;
    proc_694();
    IF (unresolved_global_1816 != 2)
        proc_695();
    END
    IF (unresolved_global_1816 == 3)
        proc_696(unresolved_global_1728, unresolved_global_1732, 3);
    END
    proc_697();
END

PROCESS proc_693(INT param0, INT param1)
PRIVATE
    INT unresolved_private_4_8;  // offset 8
BEGIN
    graph = 312;
    resolution = 10;
    file = unresolved_global_70532;
    x = param0 * 160;
    y = param1 * 160 + 160;
    REPEAT
        unresolved_private_4_8++;
        IF (unresolved_private_4_8 > 1)
            unresolved_private_4_8 = 0;
            graph++;
        END
        IF (graph > 317)
            graph = 312;
        END
        FRAME;
    UNTIL (unresolved_global_69208[param0][param1] == 0);
END

PROCESS proc_694()
PRIVATE
    INT unresolved_private_5_0;  // offset 0
    INT unresolved_private_5_4;  // offset 4
    INT unresolved_private_5_8;  // offset 8
BEGIN
    priority = 9;
    unresolved_private_5_4 = unresolved_global_1808 - 1;
    IF (unresolved_private_5_4 > 14)
        unresolved_private_5_4 = 14;
    END
    resolution = 10;
    file = unresolved_global_70532;
    IF (unresolved_global_1816 == 2)
        unresolved_global_1812 = unresolved_global_1808 + 9;
        IF (unresolved_global_1812 > 25)
            unresolved_global_1812 = 25;
        END
        unresolved_private_5_0 = unresolved_global_1812;
    END
    LOOP
        IF (unresolved_global_1812 == 100 && unresolved_global_1680 == 1)
            proc_701(unresolved_global_1768, unresolved_global_1772, 0, unresolved_private_5_4);
            proc_741(unresolved_global_1768, unresolved_global_1772);
            unresolved_global_1680 = 0;
        END
        IF (unresolved_global_1812 == 50 && unresolved_global_1684 == 1)
            proc_701(unresolved_global_1768, unresolved_global_1772, 0, unresolved_private_5_4);
            proc_741(unresolved_global_1768, unresolved_global_1772);
            unresolved_global_1684 = 0;
        END
        IF (unresolved_global_1816 == 2)
            unresolved_private_5_4 = rand(unresolved_global_1808, unresolved_global_1808 + 4);
            IF (unresolved_private_5_4 > 14)
                unresolved_private_5_4 = 14;
            END
            unresolved_private_5_8++;
            IF (unresolved_private_5_8 > 30)
                unresolved_private_5_8 = 0;
                IF (unresolved_private_5_0 > 0)
                    proc_701(unresolved_global_1768, unresolved_global_1772, 0, unresolved_private_5_4);
                    proc_741(unresolved_global_1768, unresolved_global_1772);
                    unresolved_private_5_0 = unresolved_private_5_0 - 1;
                END
            END
        END
        FRAME;
    END
END

PROCESS proc_695()
PRIVATE
    INT unresolved_private_6_0;  // offset 0
    INT unresolved_private_6_4;  // offset 4
    INT unresolved_private_6_8;  // offset 8
    INT unresolved_private_6_12;  // offset 12
    INT unresolved_private_6_16;  // offset 16
BEGIN
    unresolved_private_6_16 = 312;
    region = 1;
    z = 512;
    file = unresolved_global_70532;
    x = 160;
    y = 120;
    graph = 502;
    unresolved_private_6_0 = 1;
    LOOP
        unresolved_global_1812 = 0;
        map_clear(unresolved_global_70532, 502, 0);
        unresolved_private_6_4 = 0;
        IF (unresolved_private_6_12 > 1)
            unresolved_private_6_0++;
            unresolved_private_6_16++;
            unresolved_private_6_12 = 0;
        END
        unresolved_private_6_12++;
        IF (unresolved_private_6_16 > 317)
            unresolved_private_6_16 = 312;
        END
        IF (unresolved_private_6_0 > 12)
            unresolved_private_6_0 = 1;
        END
        FOR (y = 1; y < 14; y = y + 1)
            IF (unresolved_private_6_4 > 11)
                unresolved_private_6_4 = 0;
            END
            unresolved_private_6_8 = unresolved_private_6_4 + unresolved_private_6_0;
            unresolved_private_6_4++;
            FOR (x = 1; x < 20; x = x + 1)
                IF (unresolved_private_6_8 > 12)
                    unresolved_private_6_8 = unresolved_private_6_8 - 12;
                END
                IF (unresolved_global_69208[x][y] == 1 && timer[0] < 1000)
                    map_put(unresolved_global_70532, 502, unresolved_private_6_8 + 319, x * 16 - 8, y * 16 - 8);
                    unresolved_global_1812++;
                END
                IF (unresolved_global_69208[x][y] == 1 && timer[0] >= 1000)
                    map_put(unresolved_global_70532, 502, unresolved_private_6_8 + 299, x * 16 - 8, y * 16 - 8);
                    unresolved_global_1812++;
                END
                IF (unresolved_global_69208[x][y] == 2)
                    map_put(unresolved_global_70532, 502, unresolved_private_6_16, x * 16 - 8, y * 16 - 8);
                    unresolved_global_1812++;
                END
                unresolved_private_6_8++;
            END
        END
        x = 160;
        y = 128;
        FRAME;
        IF (unresolved_global_1812 == 0 && unresolved_global_1816 == 1)
            proc_708();
            BREAK;
        END
        IF (unresolved_global_1812 == 0 && unresolved_global_1816 == 4)
            proc_708();
            BREAK;
        END
    END
END

PROCESS proc_696(INT param0, INT param1, INT param2)
PRIVATE
    INT unresolved_private_7_12;  // offset 12
    INT unresolved_private_7_16;  // offset 16
    INT unresolved_private_7_20;  // offset 20
    INT unresolved_private_7_24;  // offset 24
    INT unresolved_private_7_28;  // offset 28
    INT unresolved_private_7_36;  // offset 36
    INT unresolved_private_7_40;  // offset 40
    INT unresolved_private_7_44;  // offset 44
BEGIN
    x = param0;
    y = param1;
    unresolved_private_7_12 = param2;
    unresolved_private_7_28 = 80;
    unresolved_private_7_36 = 1;
    resolution = 10;
    file = unresolved_global_70532;
    graph = 636;
    region = 1;
    unresolved_private_7_24 = 495;
    unresolved_private_7_40 = 25;
    LOOP
        IF (unresolved_global_1812 == 0 && unresolved_private_7_40 == 0)
            play_wav(unresolved_global_70648, 0);
            unresolved_private_7_44 = unresolved_global_1808 - 1;
            IF (unresolved_private_7_44 > 14)
                unresolved_private_7_44 = 14;
            END
            unresolved_global_1808++;
            unresolved_global_1776 = 19 + unresolved_global_1808;
            IF (unresolved_global_1776 > 40)
                unresolved_global_1776 = 40;
            END
            proc_701(unresolved_global_1768, unresolved_global_1772, 1, unresolved_private_7_44);
            unresolved_private_7_40 = 25;
        END
        graph = 0;
        IF (unresolved_private_7_40 > 0)
            graph = unresolved_private_7_24;
            unresolved_private_7_24++;
            IF (unresolved_private_7_24 > 497)
                unresolved_private_7_24 = 495;
            END
        END
        FOR (unresolved_private_7_20 = 0; unresolved_private_7_20 < unresolved_private_7_28; unresolved_private_7_20++)
            unresolved_private_7_16 = rand(1, 2);
            IF (x % 160 == 0 && y % 160 == 0)
                IF (unresolved_private_7_40 > 0 && (unresolved_global_69208[x / 160][(y - 160) / 160] == 0 && (x / 160 >= 1 && (x / 160 <= 19 && ((y - 160) / 160 >= 1 && (y - 160) / 160 <= 13)))))
                    unresolved_global_69208[x / 160][(y - 160) / 160] = 1;
                    IF (unresolved_private_7_40 == 13 && unresolved_global_1808 % 2 == 1)
                        unresolved_global_69208[x / 160][(y - 160) / 160] = 2;
                    END
                    unresolved_private_7_40 = unresolved_private_7_40 - 1;
                    proc_741(x, y);
                END
                IF (x / 160 < 21 && (x / 160 > 0 && ((y - 160) / 160 < 14 && (y - 160) / 160 > 0)))
                    IF (param2 == 1 || param2 == 3)
                        IF (unresolved_private_7_16 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                            unresolved_private_7_12 = 4;
                        END
                        IF (unresolved_private_7_16 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                            unresolved_private_7_12 = 2;
                        END
                    END
                    IF (param2 == 2 || param2 == 4)
                        IF (unresolved_private_7_16 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                            unresolved_private_7_12 = 1;
                        END
                        IF (unresolved_private_7_16 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                            unresolved_private_7_12 = 3;
                        END
                    END
                    IF (param2 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_7_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                unresolved_private_7_12 = 4;
                            ELSE
                                unresolved_private_7_12 = 2;
                            END
                        END
                        IF (unresolved_private_7_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                unresolved_private_7_12 = 2;
                            ELSE
                                unresolved_private_7_12 = 4;
                            END
                        END
                    END
                    IF (param2 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] > 0)
                        IF (unresolved_private_7_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                unresolved_private_7_12 = 1;
                            ELSE
                                unresolved_private_7_12 = 3;
                            END
                        END
                        IF (unresolved_private_7_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                unresolved_private_7_12 = 3;
                            ELSE
                                unresolved_private_7_12 = 1;
                            END
                        END
                    END
                    IF (param2 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_7_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                unresolved_private_7_12 = 4;
                            ELSE
                                unresolved_private_7_12 = 2;
                            END
                        END
                        IF (unresolved_private_7_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                unresolved_private_7_12 = 2;
                            ELSE
                                unresolved_private_7_12 = 4;
                            END
                        END
                    END
                    IF (param2 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] > 0)
                        IF (unresolved_private_7_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                unresolved_private_7_12 = 1;
                            ELSE
                                unresolved_private_7_12 = 3;
                            END
                        END
                        IF (unresolved_private_7_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                unresolved_private_7_12 = 3;
                            ELSE
                                unresolved_private_7_12 = 1;
                            END
                        END
                    END
                END
                IF (x == unresolved_global_1768 && y == unresolved_global_1772)
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] == 4)
                        unresolved_private_7_12 = 1;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] == 4)
                        unresolved_private_7_12 = 2;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] == 4)
                        unresolved_private_7_12 = 3;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] == 4)
                        unresolved_private_7_12 = 4;
                    END
                END
            END
            param2 = unresolved_private_7_12;
            IF (param2 == 1)
                x = x + 1;
            END
            IF (param2 == 2)
                y = y + 1;
            END
            IF (param2 == 3)
                x = x - 1;
            END
            IF (param2 == 4)
                y = y - 1;
            END
            IF (y > 2480)
                y = 8;
            END
            IF (y < 8)
                y = 2480;
            END
            IF (x > 3280)
                x = -8;
            END
            IF (x < -8)
                x = 3280;
            END
        END
        FRAME;
    END
END

PROCESS proc_697()
PRIVATE
    INT unresolved_private_8_0;  // offset 0
BEGIN
    timer[0] = 1000;
    proc_699(unresolved_global_1736, unresolved_global_1740);
    IF (unresolved_global_1888[unresolved_global_1784][unresolved_global_1768 / 160][(unresolved_global_1772 - 160) / 160 - 1] == 4)
        proc_700(unresolved_global_1768, unresolved_global_1772, 1, 1, 320);
        proc_700(unresolved_global_1768 - 160, unresolved_global_1772, 2, 1, 640);
        proc_700(unresolved_global_1768 + 160, unresolved_global_1772, 3, 3, 1280);
        proc_700(unresolved_global_1768, unresolved_global_1772 - 320, 4, 1, 160);
    END
    IF (unresolved_global_1888[unresolved_global_1784][unresolved_global_1768 / 160][(unresolved_global_1772 - 160) / 160 + 1] == 4)
        proc_700(unresolved_global_1768, unresolved_global_1772, 1, 1, 320);
        proc_700(unresolved_global_1768 - 160, unresolved_global_1772, 2, 1, 640);
        proc_700(unresolved_global_1768 + 160, unresolved_global_1772, 3, 3, 1280);
        proc_700(unresolved_global_1768, unresolved_global_1772 + 320, 4, 1, 160);
    END
    IF (unresolved_global_1888[unresolved_global_1784][unresolved_global_1768 / 160 + 1][(unresolved_global_1772 - 160) / 160] == 4)
        proc_700(unresolved_global_1768, unresolved_global_1772, 1, 1, 320);
        proc_700(unresolved_global_1768, unresolved_global_1772 - 160, 2, 1, 640);
        proc_700(unresolved_global_1768, unresolved_global_1772 + 160, 3, 3, 1280);
        proc_700(unresolved_global_1768 + 320, unresolved_global_1772, 4, 1, 160);
    END
    IF (unresolved_global_1888[unresolved_global_1784][unresolved_global_1768 / 160 - 1][(unresolved_global_1772 - 160) / 160] == 4)
        proc_700(unresolved_global_1768, unresolved_global_1772, 1, 1, 320);
        proc_700(unresolved_global_1768, unresolved_global_1772 - 160, 2, 1, 640);
        proc_700(unresolved_global_1768, unresolved_global_1772 + 160, 3, 3, 1280);
        proc_700(unresolved_global_1768 - 320, unresolved_global_1772, 4, 1, 160);
    END
    signal(TYPE proc_700, 3);
    signal(TYPE proc_699, 3);
    FRAME;
    file = unresolved_global_70532;
    graph = 510;
    x = 160;
    z = -257;
    FOR (y = 0; y < 120; y = y + 20)
        FRAME;
    END
    FOR (graph = 510; graph < 515; graph++)
        FRAME;
    END
    FOR (graph = 514; graph > 509; graph = graph - 1)
        FRAME;
    END
    FOR (graph = 510; graph < 513; graph++)
        FRAME;
    END
    FOR (unresolved_private_8_0 = 0; unresolved_private_8_0 < 20; unresolved_private_8_0++)
        FRAME;
    END
    FOR (x = 160; x < 440; x = x + 20)
        IF (graph < 514)
            graph++;
        END
        FRAME;
    END
    signal(TYPE proc_700, 1);
    signal(TYPE proc_699, 1);
    signal(TYPE proc_701, 1);
    signal(TYPE proc_694, 1);
    IF (unresolved_global_1816 != 4)
        proc_702(unresolved_global_1768, unresolved_global_1772, 4, 0);
    END
END

PROCESS proc_699(INT param0, INT param1)
PRIVATE
    INT unresolved_private_9_8;  // offset 8
    INT unresolved_private_9_12;  // offset 12
    INT unresolved_private_9_16;  // offset 16
    INT unresolved_private_9_20;  // offset 20
    INT unresolved_private_9_24;  // offset 24
    INT unresolved_private_9_28;  // offset 28
    INT unresolved_private_9_32;  // offset 32
BEGIN
    x = param0;
    y = param1;
    priority = 10;
    unresolved_global_1780 = 0;
    resolution = 10;
    file = unresolved_global_70532;
    graph = 11;
    region = 1;
    z = -256;
    unresolved_private_9_12 = 1;
    unresolved_private_9_20 = 1;
    REPEAT
        FRAME;
    UNTIL (exists(TYPE proc_697) == 0);
    LOOP
        unresolved_global_1804 = 0;
        IF (unresolved_global_1884 == 0)
            unresolved_private_9_8 = 0;
        END
        IF (unresolved_global_1780 > 0)
            graph = unresolved_private_9_12 + (unresolved_global_1780 - 1) * 8;
        END
        IF (unresolved_global_1780 > 0)
            unresolved_private_9_12++;
        END
        IF (unresolved_private_9_12 > 8)
            unresolved_private_9_12 = 1;
        END
        IF (unresolved_private_9_28 == 1 && is_playing_wav(unresolved_private_9_24) == 0)
            IF (unresolved_private_9_32 == 0)
                unresolved_private_9_24 = play_wav(unresolved_global_70664, 0);
            END
            IF (unresolved_private_9_32 == 1)
                unresolved_private_9_24 = play_wav(unresolved_global_70668, 0);
            END
            unresolved_private_9_32++;
            IF (unresolved_private_9_32 > 1)
                unresolved_private_9_32 = 0;
            END
        END
        FOR (unresolved_private_9_16 = 0; unresolved_private_9_16 < unresolved_global_1776; unresolved_private_9_16++)
            IF (key(77) || key(56))
                unresolved_private_9_8 = 1;
            END
            IF (key(80) || key(45))
                unresolved_private_9_8 = 2;
            END
            IF (key(75) || key(29))
                unresolved_private_9_8 = 3;
            END
            IF (key(72) || key(44))
                unresolved_private_9_8 = 4;
            END
            IF (unresolved_global_1780 == 1 && unresolved_private_9_8 == 3)
                unresolved_global_1780 = 3;
            END
            IF (unresolved_global_1780 == 3 && unresolved_private_9_8 == 1)
                unresolved_global_1780 = 1;
            END
            IF (unresolved_global_1780 == 2 && unresolved_private_9_8 == 4)
                unresolved_global_1780 = 4;
            END
            IF (unresolved_global_1780 == 4 && unresolved_private_9_8 == 2)
                unresolved_global_1780 = 2;
            END
            IF (x % 160 == 0 && y % 160 == 0)
                IF (unresolved_global_69208[x / 160][(y - 160) / 160] == 0)
                    unresolved_private_9_28 = 0;
                END
                IF (unresolved_global_69208[x / 160][(y - 160) / 160] == 1)
                    unresolved_global_69208[x / 160][(y - 160) / 160] = 0;
                    unresolved_private_9_28 = 1;
                    IF (timer[0] < 1000)
                        unresolved_global_1692 = unresolved_global_1692 + 50;
                    ELSE
                        unresolved_global_1692 = unresolved_global_1692 + 10;
                    END
                END
                IF (unresolved_global_69208[x / 160][(y - 160) / 160] == 2)
                    unresolved_global_69208[x / 160][(y - 160) / 160] = 0;
                    unresolved_global_1804 = 1;
                    play_wav(unresolved_global_70636, 0);
                    unresolved_global_1724 = 200;
                    unresolved_global_1692 = unresolved_global_1692 + 50;
                    timer[1] = 0;
                    timer[2] = 0;
                    timer[3] = 0;
                    timer[4] = 0;
                END
                IF (x / 160 < 21 && (x / 160 > 0 && ((y - 160) / 160 < 14 && (y - 160) / 160 > 0)))
                    IF (unresolved_private_9_8 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                        unresolved_global_1780 = 1;
                    END
                    IF (unresolved_private_9_8 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                        unresolved_global_1780 = 2;
                    END
                    IF (unresolved_private_9_8 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                        unresolved_global_1780 = 3;
                    END
                    IF (unresolved_private_9_8 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                        unresolved_global_1780 = 4;
                    END
                    IF (unresolved_global_1780 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] > 0)
                        unresolved_global_1780 = 0;
                    END
                    IF (unresolved_global_1780 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] > 0)
                        unresolved_global_1780 = 0;
                    END
                    IF (unresolved_global_1780 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] > 0)
                        unresolved_global_1780 = 0;
                    END
                    IF (unresolved_global_1780 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] > 0)
                        unresolved_global_1780 = 0;
                    END
                END
            END
            IF (unresolved_global_1780 == 1)
                x = x + 1;
            END
            IF (unresolved_global_1780 == 2)
                y = y + 1;
            END
            IF (unresolved_global_1780 == 3)
                x = x - 1;
            END
            IF (unresolved_global_1780 == 4)
                y = y - 1;
            END
            IF (y > 2480)
                y = 8;
            END
            IF (y < 8)
                y = 2480;
            END
            IF (x > 3280)
                x = -8;
            END
            IF (x < -8)
                x = 3280;
            END
        END
        FRAME;
        unresolved_global_1744 = x;
        unresolved_global_1748 = y;
    END
END

PROCESS proc_700(INT param0, INT param1, INT param2, INT param3, INT param4)
PRIVATE
    INT unresolved_private_10_20;  // offset 20
    INT unresolved_private_10_24;  // offset 24
    INT unresolved_private_10_28;  // offset 28
    INT unresolved_private_10_32;  // offset 32
    INT unresolved_private_10_40;  // offset 40
    INT unresolved_private_10_44;  // offset 44
    INT unresolved_private_10_48;  // offset 48
    INT unresolved_private_10_52;  // offset 52
    INT unresolved_private_10_56;  // offset 56
    INT unresolved_private_10_60;  // offset 60
    INT unresolved_private_10_64;  // offset 64
    INT unresolved_private_10_68;  // offset 68
    INT unresolved_private_10_72;  // offset 72
    INT unresolved_private_10_76;  // offset 76
    INT unresolved_private_10_80;  // offset 80
    INT unresolved_private_10_84;  // offset 84
    INT unresolved_private_10_88;  // offset 88
    INT unresolved_private_10_92;  // offset 92
    INT unresolved_private_10_96;  // offset 96
BEGIN
    x = param0;
    y = param1;
    unresolved_private_10_32 = 0;
    unresolved_private_10_20 = unresolved_global_1776;
    IF (param2 == 1)
        unresolved_private_10_64 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 5) / 100;
        unresolved_private_10_48 = 100;
        unresolved_private_10_72 = 148;
        unresolved_private_10_76 = 364;
    END
    IF (param2 == 2)
        unresolved_private_10_64 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 10) / 100;
        unresolved_private_10_48 = 100;
        unresolved_private_10_72 = 116;
        unresolved_private_10_76 = 348;
    END
    IF (param2 == 3)
        unresolved_private_10_64 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 15) / 100;
        unresolved_private_10_48 = 60;
        unresolved_private_10_72 = 132;
        unresolved_private_10_76 = 356;
    END
    IF (param2 == 4)
        unresolved_private_10_64 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 10) / 100;
        unresolved_private_10_48 = 50;
        unresolved_private_10_72 = 100;
        unresolved_private_10_76 = 340;
    END
    unresolved_private_10_68 = unresolved_private_10_64 / 2;
    unresolved_private_10_60 = unresolved_private_10_64;
    resolution = 10;
    file = unresolved_global_70532;
    graph = unresolved_private_10_76 + rand(0, 7);
    region = 1;
    z = -254;
    FOR (unresolved_private_10_32 = 0; unresolved_private_10_32 < param4; unresolved_private_10_32 = unresolved_private_10_32 + unresolved_private_10_60)
        IF (unresolved_private_10_96 > 1)
            graph++;
            unresolved_private_10_96 = 0;
        END
        unresolved_private_10_96++;
        IF (graph > unresolved_private_10_76 + 7)
            graph = unresolved_private_10_76;
        END
        FRAME;
    END
    graph = unresolved_private_10_72;
    LOOP
        IF (unresolved_global_1776 != unresolved_private_10_20)
            IF (param2 == 1)
                unresolved_private_10_64 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 5) / 100;
                unresolved_private_10_48 = 100;
                unresolved_private_10_72 = 148;
            END
            IF (param2 == 2)
                unresolved_private_10_64 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 10) / 100;
                unresolved_private_10_48 = 100;
                unresolved_private_10_72 = 116;
            END
            IF (param2 == 3)
                unresolved_private_10_64 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 15) / 100;
                unresolved_private_10_48 = 60;
                unresolved_private_10_72 = 132;
            END
            IF (param2 == 4)
                unresolved_private_10_64 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 10) / 100;
                unresolved_private_10_48 = 50;
                unresolved_private_10_72 = 100;
            END
            unresolved_private_10_68 = unresolved_private_10_64 / 2;
            unresolved_private_10_60 = unresolved_private_10_64;
            unresolved_private_10_20 = unresolved_global_1776;
        END
        unresolved_private_10_24 = 0;
        unresolved_private_10_80 = 0;
        IF (param2 == 1)
            IF (unresolved_global_1780 == 1)
                unresolved_private_10_40 = unresolved_global_1744 + 480;
                unresolved_private_10_44 = unresolved_global_1748;
            END
            IF (unresolved_global_1780 == 2)
                unresolved_private_10_40 = unresolved_global_1744;
                unresolved_private_10_44 = unresolved_global_1748 + 480;
            END
            IF (unresolved_global_1780 == 3)
                unresolved_private_10_40 = unresolved_global_1744 - 480;
                unresolved_private_10_44 = unresolved_global_1748;
            END
            IF (unresolved_global_1780 == 4)
                unresolved_private_10_40 = unresolved_global_1744;
                unresolved_private_10_44 = unresolved_global_1748 - 480;
            END
        END
        IF (param2 == 2)
            IF (unresolved_global_1780 == 1)
                unresolved_private_10_40 = unresolved_global_1744 - 480;
                unresolved_private_10_44 = unresolved_global_1748;
            END
            IF (unresolved_global_1780 == 2)
                unresolved_private_10_40 = unresolved_global_1744;
                unresolved_private_10_44 = unresolved_global_1748 - 480;
            END
            IF (unresolved_global_1780 == 3)
                unresolved_private_10_40 = unresolved_global_1744 + 480;
                unresolved_private_10_44 = unresolved_global_1748;
            END
            IF (unresolved_global_1780 == 4)
                unresolved_private_10_40 = unresolved_global_1744;
                unresolved_private_10_44 = unresolved_global_1748 + 480;
            END
        END
        IF (param2 == 3)
            unresolved_private_10_40 = unresolved_global_1744;
            unresolved_private_10_44 = unresolved_global_1748;
        END
        IF (param2 == 4 && rand(0, 100) > 95)
            unresolved_private_10_40 = unresolved_global_1744;
            unresolved_private_10_44 = unresolved_global_1748;
        END
        IF (unresolved_private_10_96 > 1)
            unresolved_private_10_28--;
            unresolved_private_10_96 = 0;
        END
        unresolved_private_10_96++;
        IF (unresolved_private_10_28 < 0)
            unresolved_private_10_28 = 3;
        END
        graph = unresolved_private_10_28 + unresolved_private_10_72 + (param3 - 1) * 4;
        IF (x > 159 && (x < 3041 && (y > 319 && y < 2241)))
            unresolved_private_10_60 = unresolved_private_10_64;
        ELSE
            unresolved_private_10_60 = unresolved_private_10_68;
        END
        FOR (unresolved_private_10_32 = 0; unresolved_private_10_32 < unresolved_private_10_60; unresolved_private_10_32++)
            IF (x > unresolved_global_1744 - 60 && (x < unresolved_global_1744 + 60 && (y > unresolved_global_1748 - 60 && y < unresolved_global_1748 + 60)))
                unresolved_private_10_80 = 1;
            END
            IF (x > unresolved_global_1752 - 60 && (x < unresolved_global_1752 + 60 && (y > unresolved_global_1756 - 60 && y < unresolved_global_1756 + 60)))
                unresolved_private_10_84 = 1;
            END
            IF (x > unresolved_global_1760 - 60 && (x < unresolved_global_1760 + 60 && (y > unresolved_global_1764 - 60 && (y < unresolved_global_1764 + 60 && exists(TYPE proc_735) == 0))))
                unresolved_private_10_88 = 1;
            END
            IF (x > unresolved_global_1760 - 480 && (x < unresolved_global_1760 + 480 && (y > unresolved_global_1764 - 480 && (y < unresolved_global_1764 + 480 && exists(TYPE proc_735) == 1))))
                unresolved_private_10_92 = 1;
            END
            IF (x % 160 == 0 && y % 160 == 0)
                IF (x == unresolved_global_1768 && y == unresolved_global_1772)
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] == 4)
                        param3 = 1;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] == 4)
                        param3 = 2;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] == 4)
                        param3 = 3;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] == 4)
                        param3 = 4;
                    END
                END
                IF (x / 160 < 21 && (x / 160 > 0 && ((y - 160) / 160 < 14 && ((y - 160) / 160 > 0 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160] < 1))))
                    IF (unresolved_private_10_48 > rand(0, 99))
                        IF (x >= unresolved_private_10_40)
                            unresolved_private_10_52 = x - unresolved_private_10_40;
                        END
                        IF (x < unresolved_private_10_40)
                            unresolved_private_10_52 = unresolved_private_10_40 - x;
                        END
                        IF (y >= unresolved_private_10_44)
                            unresolved_private_10_56 = y - unresolved_private_10_44;
                        END
                        IF (y < unresolved_private_10_44)
                            unresolved_private_10_56 = unresolved_private_10_44 - y;
                        END
                        IF (param3 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                            IF (unresolved_private_10_44 <= y && unresolved_private_10_56 >= unresolved_private_10_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                    param3 = 4;
                                END
                            END
                            IF (unresolved_private_10_44 > y && unresolved_private_10_56 >= unresolved_private_10_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                    param3 = 2;
                                END
                            END
                        END
                        IF (param3 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                            IF (unresolved_private_10_40 <= x && unresolved_private_10_52 >= unresolved_private_10_56)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                    param3 = 3;
                                END
                            END
                            IF (unresolved_private_10_40 > x && unresolved_private_10_52 >= unresolved_private_10_56)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                    param3 = 1;
                                END
                            END
                        END
                        IF (param3 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                            IF (unresolved_private_10_44 <= y && unresolved_private_10_56 >= unresolved_private_10_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                    param3 = 4;
                                END
                            END
                            IF (unresolved_private_10_44 > y && unresolved_private_10_56 >= unresolved_private_10_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                    param3 = 2;
                                END
                            END
                        END
                        IF (param3 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                            IF (unresolved_private_10_40 <= x && unresolved_private_10_52 >= unresolved_private_10_56)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                    param3 = 3;
                                END
                            END
                            IF (unresolved_private_10_40 > x && unresolved_private_10_52 >= unresolved_private_10_56)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                    param3 = 1;
                                END
                            END
                        END
                    END
                    IF (param3 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_10_44 <= y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                param3 = 4;
                            ELSE
                                param3 = 2;
                            END
                        END
                        IF (unresolved_private_10_44 > y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                param3 = 2;
                            ELSE
                                param3 = 4;
                            END
                        END
                    END
                    IF (param3 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] > 0)
                        IF (unresolved_private_10_40 <= x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                param3 = 3;
                            ELSE
                                param3 = 1;
                            END
                        END
                        IF (unresolved_private_10_40 > x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                param3 = 1;
                            ELSE
                                param3 = 3;
                            END
                        END
                    END
                    IF (param3 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_10_44 <= y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                param3 = 4;
                            ELSE
                                param3 = 2;
                            END
                        END
                        IF (unresolved_private_10_44 > y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                param3 = 2;
                            ELSE
                                param3 = 4;
                            END
                        END
                    END
                    IF (param3 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] > 0)
                        IF (unresolved_private_10_40 <= x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                param3 = 3;
                            ELSE
                                param3 = 1;
                            END
                        END
                        IF (unresolved_private_10_40 > x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                param3 = 1;
                            ELSE
                                param3 = 3;
                            END
                        END
                    END
                END
                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160] != 4)
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] == 5)
                        param3 = 1;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] == 5)
                        param3 = 2;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] == 5)
                        param3 = 3;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] == 5)
                        param3 = 4;
                    END
                END
            END
            IF (param3 == 1)
                x = x + 1;
            END
            IF (param3 == 2)
                y = y + 1;
            END
            IF (param3 == 3)
                x = x - 1;
            END
            IF (param3 == 4)
                y = y - 1;
            END
            IF (y > 2480)
                y = 8;
            END
            IF (y < 8)
                y = 2480;
            END
            IF (x > 3280)
                x = -8;
            END
            IF (x < -8)
                x = 3280;
            END
        END
        FRAME;
        IF (unresolved_private_10_80 == 1)
            proc_736();
        END
        IF (unresolved_private_10_84 == 1)
            timer[param2] = 0;
            IF (param3 == 1)
                proc_737(x, y, param2, 3, 1);
            END
            IF (param3 == 2)
                proc_737(x, y, param2, 4, 1);
            END
            IF (param3 == 3)
                proc_737(x, y, param2, 1, 1);
            END
            IF (param3 == 4)
                proc_737(x, y, param2, 2, 1);
            END
            BREAK;
        END
        IF (unresolved_global_1804 == 1)
            IF (param3 == 1)
                proc_737(x, y, param2, 3, 0);
            END
            IF (param3 == 2)
                proc_737(x, y, param2, 4, 0);
            END
            IF (param3 == 3)
                proc_737(x, y, param2, 1, 0);
            END
            IF (param3 == 4)
                proc_737(x, y, param2, 2, 0);
            END
            BREAK;
        END
        IF (unresolved_global_1804 == 2)
            proc_738(x, y);
            proc_739(x, y, param2, param3);
            BREAK;
        END
        IF (unresolved_global_1804 == 3)
            proc_740(x, y, param2, param3);
            proc_741(x, y);
            BREAK;
        END
        IF (unresolved_private_10_88 == 1)
            proc_735(x, y);
            proc_742(x, y, param2, param3);
            proc_743(x, y, 464);
            unresolved_global_1692 = unresolved_global_1692 + 500;
            signal(TYPE proc_744, 0);
            BREAK;
        END
        IF (unresolved_private_10_92 == 1)
            proc_742(x, y, param2, param3);
            proc_743(x, y, 464);
            unresolved_global_1692 = unresolved_global_1692 + 500;
            BREAK;
        END
    END
END

PROCESS proc_702(INT param0, INT param1, INT param2, INT param3)
PRIVATE
    INT unresolved_private_11_16;  // offset 16
    INT unresolved_private_11_20;  // offset 20
    INT unresolved_private_11_24;  // offset 24
    INT unresolved_private_11_28;  // offset 28
    INT unresolved_private_11_32;  // offset 32
    INT unresolved_private_11_36;  // offset 36
    INT unresolved_private_11_40;  // offset 40
BEGIN
    x = param0;
    y = param1;
    priority = 9;
    unresolved_private_11_36 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 30) / 100;
    unresolved_private_11_32 = rand(0, 4);
    resolution = 10;
    file = unresolved_global_70532;
    graph = 636;
    region = 1;
    param3 = 0;
    LOOP
        param3++;
        IF (param3 > 60 && unresolved_global_1816 != 2)
            param3 = 0;
            unresolved_private_11_32 = rand(0, 4);
        END
        IF (param3 > 60 && unresolved_global_1816 == 2)
            param3 = 0;
            unresolved_private_11_32 = rand(1, 4);
        END
        graph = unresolved_private_11_32 * 8 + 630 + unresolved_private_11_28;
        unresolved_private_11_28++;
        IF (unresolved_private_11_28 > 7)
            unresolved_private_11_28 = 0;
        END
        FOR (unresolved_private_11_24 = 0; unresolved_private_11_24 < unresolved_private_11_36; unresolved_private_11_24++)
            unresolved_private_11_20 = rand(1, 2);
            IF (x > unresolved_global_1744 - 60 && (x < unresolved_global_1744 + 60 && (y > unresolved_global_1748 - 60 && y < unresolved_global_1748 + 60)))
                unresolved_private_11_40 = 1;
            END
            IF (x % 160 == 0 && y % 160 == 0)
                IF (x / 160 < 21 && (x / 160 > 0 && ((y - 160) / 160 < 14 && (y - 160) / 160 > 0)))
                    IF (param2 == 1 || param2 == 3)
                        IF (unresolved_private_11_20 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                            unresolved_private_11_16 = 4;
                        END
                        IF (unresolved_private_11_20 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                            unresolved_private_11_16 = 2;
                        END
                    END
                    IF (param2 == 2 || param2 == 4)
                        IF (unresolved_private_11_20 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                            unresolved_private_11_16 = 1;
                        END
                        IF (unresolved_private_11_20 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                            unresolved_private_11_16 = 3;
                        END
                    END
                    IF (param2 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_11_20 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                unresolved_private_11_16 = 4;
                            ELSE
                                unresolved_private_11_16 = 2;
                            END
                        END
                        IF (unresolved_private_11_20 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                unresolved_private_11_16 = 2;
                            ELSE
                                unresolved_private_11_16 = 4;
                            END
                        END
                    END
                    IF (param2 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] > 0)
                        IF (unresolved_private_11_20 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                unresolved_private_11_16 = 1;
                            ELSE
                                unresolved_private_11_16 = 3;
                            END
                        END
                        IF (unresolved_private_11_20 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                unresolved_private_11_16 = 3;
                            ELSE
                                unresolved_private_11_16 = 1;
                            END
                        END
                    END
                    IF (param2 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_11_20 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                unresolved_private_11_16 = 4;
                            ELSE
                                unresolved_private_11_16 = 2;
                            END
                        END
                        IF (unresolved_private_11_20 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                unresolved_private_11_16 = 2;
                            ELSE
                                unresolved_private_11_16 = 4;
                            END
                        END
                    END
                    IF (param2 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] > 0)
                        IF (unresolved_private_11_20 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                unresolved_private_11_16 = 1;
                            ELSE
                                unresolved_private_11_16 = 3;
                            END
                        END
                        IF (unresolved_private_11_20 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                unresolved_private_11_16 = 3;
                            ELSE
                                unresolved_private_11_16 = 1;
                            END
                        END
                    END
                END
                IF (x == unresolved_global_1768 && y == unresolved_global_1772)
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] == 4)
                        unresolved_private_11_16 = 1;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] == 4)
                        unresolved_private_11_16 = 2;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] == 4)
                        unresolved_private_11_16 = 3;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] == 4)
                        unresolved_private_11_16 = 4;
                    END
                END
            END
            param2 = unresolved_private_11_16;
            IF (param2 == 1)
                x = x + 1;
            END
            IF (param2 == 2)
                y = y + 1;
            END
            IF (param2 == 3)
                x = x - 1;
            END
            IF (param2 == 4)
                y = y - 1;
            END
            IF (y > 2480)
                y = 8;
            END
            IF (y < 8)
                y = 2480;
            END
            IF (x > 3280)
                x = -8;
            END
            IF (x < -8)
                x = 3280;
            END
        END
        FRAME;
        IF (unresolved_private_11_40 == 1)
            BREAK;
        END
    END
    proc_741(x, y);
    IF (unresolved_private_11_32 == 0)
        timer[0] = 0;
        play_wav(unresolved_global_70628, 0);
    END
    IF (unresolved_private_11_32 == 1)
        unresolved_global_1804 = 2;
        proc_758();
    END
    IF (unresolved_private_11_32 == 2)
        proc_744(x, y, param2);
        play_wav(unresolved_global_70588, 0);
    END
    IF (unresolved_private_11_32 == 3)
        proc_752(x, y, param2);
        play_wav(unresolved_global_70580, 0);
    END
    IF (unresolved_private_11_32 == 4)
        unresolved_global_1804 = 3;
        play_wav(unresolved_global_70624, 0);
    END
END

PROCESS proc_708()
PRIVATE
    INT unresolved_private_12_0;  // offset 0
    INT unresolved_private_12_4;  // offset 4
    INT unresolved_private_12_8;  // offset 8
BEGIN
    file = unresolved_global_70532;
    resolution = 10;
    x = unresolved_global_1744;
    y = unresolved_global_1748;
    z = -256;
    signal(TYPE proc_700, 3);
    signal(TYPE proc_747, 3);
    signal(TYPE proc_737, 3);
    signal(TYPE proc_749, 3);
    signal(TYPE proc_702, 3);
    signal(TYPE proc_752, 3);
    signal(TYPE proc_740, 3);
    signal(TYPE proc_744, 3);
    signal(TYPE proc_742, 3);
    signal(TYPE proc_738, 3);
    signal(TYPE proc_739, 3);
    signal(TYPE proc_701, 3);
    signal(TYPE proc_694, 3);
    signal(TYPE proc_694, 0);
    signal(TYPE proc_699, 0);
    play_song(unresolved_global_1856, 0);
    FOR (unresolved_private_12_0 = 0; unresolved_private_12_0 < 8; unresolved_private_12_0++)
        FOR (graph = 33; graph < 40; graph++)
            IF (unresolved_private_12_8 < 4)
                proc_771(900 + unresolved_private_12_8 * 160, 0 - unresolved_private_12_8 * 20, 515 + unresolved_private_12_8);
            END
            IF (unresolved_private_12_8 > 4 && unresolved_private_12_8 < 10)
                proc_771(900 + unresolved_private_12_8 * 160, 0 - unresolved_private_12_8 * 20, 515 + unresolved_private_12_8 - 1);
            END
            unresolved_private_12_8 = unresolved_private_12_8 + 1;
            FOR (unresolved_private_12_4 = 0; unresolved_private_12_4 < 2; unresolved_private_12_4++)
                FRAME;
            END
        END
    END
    graph = 0;
    signal(TYPE proc_700, 0);
    signal(TYPE proc_747, 0);
    signal(TYPE proc_737, 0);
    signal(TYPE proc_749, 0);
    signal(TYPE proc_702, 0);
    signal(TYPE proc_752, 0);
    signal(TYPE proc_740, 0);
    signal(TYPE proc_744, 0);
    signal(TYPE proc_742, 0);
    signal(TYPE proc_738, 0);
    signal(TYPE proc_739, 0);
    signal(TYPE proc_701, 0);
    signal(TYPE proc_694, 0);
    signal(TYPE proc_695, 0);
    signal(TYPE proc_771, 0);
    signal(TYPE proc_693, 0);
    unresolved_global_1752 = 0;
    unresolved_global_1756 = 0;
    unresolved_global_1760 = 0;
    unresolved_global_1764 = 0;
    FRAME;
    unresolved_global_1808 = unresolved_global_1808 + 1;
    proc_688();
END

PROCESS proc_736()
PRIVATE
    INT unresolved_private_13_0;  // offset 0
BEGIN
    file = unresolved_global_70532;
    resolution = 10;
    x = unresolved_global_1744;
    y = unresolved_global_1748 - 80;
    z = -256;
    signal(TYPE proc_700, 3);
    signal(TYPE proc_747, 3);
    signal(TYPE proc_737, 3);
    signal(TYPE proc_749, 3);
    signal(TYPE proc_702, 3);
    signal(TYPE proc_752, 3);
    signal(TYPE proc_740, 3);
    signal(TYPE proc_744, 3);
    signal(TYPE proc_742, 3);
    signal(TYPE proc_738, 3);
    signal(TYPE proc_739, 3);
    signal(TYPE proc_701, 3);
    signal(TYPE proc_694, 3);
    signal(TYPE proc_699, 0);
    play_wav(unresolved_global_70652, 0);
    FOR (graph = 40; graph < 58; graph++)
        FRAME;
    END
    FOR (graph = 40; graph < 72; graph++)
        IF (graph == 64)
            play_wav(unresolved_global_70656, 0);
        END
        FRAME;
    END
    unresolved_global_1688 = unresolved_global_1688 - 1;
    graph = 498;
    FOR (unresolved_private_13_0 = 0; unresolved_private_13_0 < 20; unresolved_private_13_0++)
        FRAME;
    END
    signal(TYPE proc_700, 0);
    signal(TYPE proc_747, 0);
    signal(TYPE proc_737, 0);
    signal(TYPE proc_749, 0);
    signal(TYPE proc_702, 0);
    signal(TYPE proc_752, 0);
    signal(TYPE proc_740, 0);
    signal(TYPE proc_744, 0);
    signal(TYPE proc_742, 0);
    signal(TYPE proc_738, 0);
    signal(TYPE proc_739, 0);
    IF (unresolved_global_1816 != 2)
        signal(TYPE proc_701, 0);
    END
    unresolved_global_1752 = 0;
    unresolved_global_1756 = 0;
    unresolved_global_1760 = 0;
    unresolved_global_1764 = 0;
    FRAME;
    IF (unresolved_global_1688 > 0)
        proc_697();
    END
    IF (unresolved_global_1688 == 0)
        proc_753();
    END
END

PROCESS proc_737(INT param0, INT param1, INT param2, INT param3, INT param4)
PRIVATE
    INT unresolved_private_14_20;  // offset 20
    INT unresolved_private_14_24;  // offset 24
    INT unresolved_private_14_28;  // offset 28
    INT unresolved_private_14_36;  // offset 36
    INT unresolved_private_14_40;  // offset 40
    INT unresolved_private_14_44;  // offset 44
    INT unresolved_private_14_48;  // offset 48
    INT unresolved_private_14_52;  // offset 52
    INT unresolved_private_14_56;  // offset 56
    INT unresolved_private_14_60;  // offset 60
    INT unresolved_private_14_68;  // offset 68
    INT unresolved_private_14_72;  // offset 72
BEGIN
    x = param0;
    y = param1;
    unresolved_private_14_68 = 1000 - unresolved_global_1808 * 60;
    IF (unresolved_private_14_68 < 0)
        unresolved_private_14_68 = 0;
    END
    resolution = 10;
    file = unresolved_global_70532;
    graph = 100;
    region = 1;
    unresolved_private_14_48 = 40;
    unresolved_private_14_60 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 50) / 100;
    LOOP
        unresolved_private_14_20 = 0;
        unresolved_private_14_72 = 0;
        unresolved_private_14_36 = unresolved_global_1744;
        unresolved_private_14_40 = unresolved_global_1748;
        IF (unresolved_private_14_44 > 3)
            unresolved_private_14_24--;
            unresolved_private_14_44 = 0;
        END
        unresolved_private_14_44++;
        IF (unresolved_private_14_24 < 0)
            unresolved_private_14_24 = 3;
        END
        graph = unresolved_private_14_24 + 164;
        IF (timer[param2] > unresolved_private_14_68 - 200)
            graph = graph + 4;
        END
        FOR (unresolved_private_14_28 = 0; unresolved_private_14_28 < unresolved_private_14_60; unresolved_private_14_28++)
            IF (x > unresolved_global_1744 - 60 && (x < unresolved_global_1744 + 60 && (y > unresolved_global_1748 - 60 && y < unresolved_global_1748 + 60)))
                unresolved_private_14_72 = 1;
            END
            IF (x % 160 == 0 && y % 160 == 0)
                IF (x == unresolved_global_1768 && y == unresolved_global_1772)
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] == 4)
                        param3 = 1;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] == 4)
                        param3 = 2;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] == 4)
                        param3 = 3;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] == 4)
                        param3 = 4;
                    END
                END
                IF (x / 160 < 21 && (x / 160 > 0 && ((y - 160) / 160 < 14 && ((y - 160) / 160 > 0 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160] < 1))))
                    IF (unresolved_private_14_48 > rand(0, 99))
                        IF (x >= unresolved_private_14_36)
                            unresolved_private_14_52 = x - unresolved_private_14_36;
                        END
                        IF (x < unresolved_private_14_36)
                            unresolved_private_14_52 = unresolved_private_14_36 - x;
                        END
                        IF (y >= unresolved_private_14_40)
                            unresolved_private_14_56 = y - unresolved_private_14_40;
                        END
                        IF (y < unresolved_private_14_40)
                            unresolved_private_14_56 = unresolved_private_14_40 - y;
                        END
                        IF (param3 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                            IF (unresolved_private_14_40 <= y && unresolved_private_14_56 <= unresolved_private_14_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                    param3 = 2;
                                END
                            END
                            IF (unresolved_private_14_40 > y && unresolved_private_14_56 <= unresolved_private_14_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                    param3 = 4;
                                END
                            END
                        END
                        IF (param3 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                            IF (unresolved_private_14_36 <= x && unresolved_private_14_52 <= unresolved_private_14_56)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                    param3 = 1;
                                END
                            END
                            IF (unresolved_private_14_36 > x && unresolved_private_14_52 <= unresolved_private_14_56)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                    param3 = 3;
                                END
                            END
                        END
                        IF (param3 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                            IF (unresolved_private_14_40 <= y && unresolved_private_14_56 <= unresolved_private_14_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                    param3 = 2;
                                END
                            END
                            IF (unresolved_private_14_40 > y && unresolved_private_14_56 <= unresolved_private_14_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                    param3 = 4;
                                END
                            END
                        END
                        IF (param3 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                            IF (unresolved_private_14_36 <= x && unresolved_private_14_52 <= unresolved_private_14_56)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                    param3 = 1;
                                END
                            END
                            IF (unresolved_private_14_36 > x && unresolved_private_14_52 <= unresolved_private_14_56)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                    param3 = 3;
                                END
                            END
                        END
                    END
                    IF (param3 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_14_40 >= y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                param3 = 4;
                            ELSE
                                param3 = 2;
                            END
                        END
                        IF (unresolved_private_14_40 < y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                param3 = 2;
                            ELSE
                                param3 = 4;
                            END
                        END
                    END
                    IF (param3 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] > 0)
                        IF (unresolved_private_14_36 >= x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                param3 = 3;
                            ELSE
                                param3 = 1;
                            END
                        END
                        IF (unresolved_private_14_36 < x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                param3 = 1;
                            ELSE
                                param3 = 3;
                            END
                        END
                    END
                    IF (param3 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_14_40 >= y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                param3 = 4;
                            ELSE
                                param3 = 2;
                            END
                        END
                        IF (unresolved_private_14_40 < y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                param3 = 2;
                            ELSE
                                param3 = 4;
                            END
                        END
                    END
                    IF (param3 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] > 0)
                        IF (unresolved_private_14_36 >= x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                param3 = 3;
                            ELSE
                                param3 = 1;
                            END
                        END
                        IF (unresolved_private_14_36 < x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                param3 = 1;
                            ELSE
                                param3 = 3;
                            END
                        END
                    END
                END
            END
            IF (param3 == 1)
                x = x + 1;
            END
            IF (param3 == 2)
                y = y + 1;
            END
            IF (param3 == 3)
                x = x - 1;
            END
            IF (param3 == 4)
                y = y - 1;
            END
            IF (y > 2480)
                y = 8;
            END
            IF (y < 8)
                y = 2480;
            END
            IF (x > 3280)
                x = -8;
            END
            IF (x < -8)
                x = 3280;
            END
        END
        FRAME;
        IF (timer[param2] > unresolved_private_14_68)
            proc_700(x, y, param2, param3, 0);
            BREAK;
        END
        IF (unresolved_private_14_72 == 1)
            proc_747(x, y, param2, param3);
            IF (param4 == 0)
                IF (unresolved_global_1724 == 200)
                    proc_743(x, y, 460);
                    unresolved_global_1692 = unresolved_global_1692 + 200;
                    play_wav(unresolved_global_70604, 0);
                END
                IF (unresolved_global_1724 == 400)
                    proc_743(x, y, 461);
                    unresolved_global_1692 = unresolved_global_1692 + 400;
                    play_wav(unresolved_global_70608, 0);
                END
                IF (unresolved_global_1724 == 800)
                    proc_743(x, y, 462);
                    unresolved_global_1692 = unresolved_global_1692 + 800;
                    play_wav(unresolved_global_70612, 0);
                END
                IF (unresolved_global_1724 == 1600)
                    proc_743(x, y, 463);
                    unresolved_global_1692 = unresolved_global_1692 + 1600;
                    play_wav(unresolved_global_70616, 0);
                END
                unresolved_global_1724 = unresolved_global_1724 * 2;
            ELSE
                proc_743(x, y, 460);
                unresolved_global_1692 = unresolved_global_1692 + 200;
            END
            BREAK;
        END
    END
END

PROCESS proc_738(INT param0, INT param1)
PRIVATE
    INT unresolved_private_15_8;  // offset 8
    INT unresolved_private_15_12;  // offset 12
    INT unresolved_private_15_16;  // offset 16
BEGIN
    x = param0;
    y = param1;
    z = -256;
    unresolved_private_15_16 = y;
    unresolved_private_15_12 = 1600;
    file = unresolved_global_70532;
    resolution = 10;
    priority = 1;
    graph = 628;
    REPEAT
        y = unresolved_private_15_16 - unresolved_private_15_12;
        unresolved_private_15_12 = unresolved_private_15_12 - unresolved_private_15_8;
        unresolved_private_15_8 = unresolved_private_15_8 + 5;
        IF (unresolved_private_15_8 > 60)
            unresolved_private_15_8 = 60;
        END
        FRAME;
    UNTIL (unresolved_private_15_12 <= 80);
END

PROCESS proc_739(INT param0, INT param1, INT param2, INT param3)
PRIVATE
    INT unresolved_private_16_16;  // offset 16
    INT unresolved_private_16_20;  // offset 20
    INT unresolved_private_16_24;  // offset 24
    INT unresolved_private_16_28;  // offset 28
BEGIN
    x = param0;
    y = param1;
    resolution = 10;
    unresolved_private_16_28 = y;
    y = y - 40;
    file = unresolved_global_70532;
    unresolved_private_16_24 = 1;
    IF (param2 == 1)
        unresolved_private_16_20 = 296;
    END
    IF (param2 == 2)
        unresolved_private_16_20 = 290;
    END
    IF (param2 == 3)
        unresolved_private_16_20 = 293;
    END
    IF (param2 == 4)
        unresolved_private_16_20 = 287;
    END
    REPEAT
        graph = unresolved_private_16_20 + unresolved_private_16_16;
        unresolved_private_16_16 = unresolved_private_16_16 + unresolved_private_16_24;
        IF (unresolved_private_16_16 > 2)
            unresolved_private_16_16 = 1;
            unresolved_private_16_24 = -1;
        END
        IF (unresolved_private_16_16 < 0)
            unresolved_private_16_16 = 1;
            unresolved_private_16_24 = 1;
        END
        FRAME;
    UNTIL (exists(TYPE proc_738) == 0);
    proc_743(x, y, 460);
    unresolved_global_1692 = unresolved_global_1692 + 200;
    unresolved_private_16_24 = 1;
    unresolved_private_16_16 = 0;
    IF (param2 == 1)
        unresolved_private_16_20 = 621;
    END
    IF (param2 == 2)
        unresolved_private_16_20 = 607;
    END
    IF (param2 == 3)
        unresolved_private_16_20 = 614;
    END
    IF (param2 == 4)
        unresolved_private_16_20 = 600;
    END
    FOR (graph = unresolved_private_16_20; graph < unresolved_private_16_20 + 7; graph++)
        FRAME;
    END
    FOR (graph = unresolved_private_16_20 + 6; graph > unresolved_private_16_20 + 1; graph--)
        FRAME;
    END
    proc_747(x, unresolved_private_16_28, param2, param3);
    LOOP
        FRAME;
        IF (x > unresolved_global_1744 - 80 && (x < unresolved_global_1744 + 80 && (unresolved_private_16_28 > unresolved_global_1748 - 80 && unresolved_private_16_28 < unresolved_global_1748 + 80)))
            proc_743(x, y, 464);
            unresolved_global_1692 = unresolved_global_1692 + 500;
            play_wav(unresolved_global_70596, 0);
            BREAK;
        END
    END
END

PROCESS proc_740(INT param0, INT param1, INT param2, INT param3)
PRIVATE
    INT unresolved_private_17_16;  // offset 16
    INT unresolved_private_17_20;  // offset 20
    INT unresolved_private_17_24;  // offset 24
    INT unresolved_private_17_28;  // offset 28
    INT unresolved_private_17_32;  // offset 32
    INT unresolved_private_17_36;  // offset 36
    INT unresolved_private_17_40;  // offset 40
BEGIN
    x = param0;
    y = param1;
    file = unresolved_global_70532;
    resolution = 10;
    IF (param2 == 1)
        unresolved_private_17_28 = 244;
        unresolved_private_17_32 = 296;
    END
    IF (param2 == 2)
        unresolved_private_17_28 = 228;
        unresolved_private_17_32 = 290;
    END
    IF (param2 == 3)
        unresolved_private_17_28 = 236;
        unresolved_private_17_32 = 293;
    END
    IF (param2 == 4)
        unresolved_private_17_28 = 220;
        unresolved_private_17_32 = 287;
    END
    FOR (unresolved_private_17_16 = 0; unresolved_private_17_16 < 5000; unresolved_private_17_16 = unresolved_private_17_16 + unresolved_global_1776)
        graph = unresolved_private_17_28 + unresolved_private_17_20;
        IF (unresolved_private_17_36 > 1)
            unresolved_private_17_20++;
            unresolved_private_17_36 = 0;
        END
        IF (unresolved_private_17_20 > 7)
            unresolved_private_17_20 = 0;
        END
        IF (unresolved_private_17_24 > 2 && unresolved_private_17_16 > 4000)
            graph = unresolved_private_17_32;
            unresolved_private_17_24 = 0;
        END
        unresolved_private_17_36++;
        unresolved_private_17_24++;
        FRAME;
        IF (x > unresolved_global_1744 - 80 && (x < unresolved_global_1744 + 80 && (y > unresolved_global_1748 - 80 && y < unresolved_global_1748 + 80)))
            unresolved_private_17_40 = 1;
            BREAK;
        END
    END
    IF (unresolved_private_17_40 == 0)
        proc_700(x, y, param2, param3, 0);
    END
    IF (unresolved_private_17_40 == 1)
        play_wav(unresolved_global_70632, 0);
        proc_747(x, y, param2, param3);
        proc_743(x, y, 465);
        unresolved_global_1692 = unresolved_global_1692 + 1000;
        FOR (graph = 252; graph < 270; graph++)
            FRAME;
        END
    END
END

PROCESS proc_741(INT param0, INT param1)
BEGIN
    x = param0;
    y = param1;
    file = unresolved_global_70532;
    resolution = 10;
    z = -256;
    FOR (graph = 483; graph < 495; graph++)
        FRAME;
    END
END

PROCESS proc_735(INT param0, INT param1)
PRIVATE
    INT unresolved_private_19_8;  // offset 8
BEGIN
    x = param0;
    y = param1;
    play_wav(unresolved_global_70592, 0);
    resolution = 10;
    file = unresolved_global_70532;
    FOR (graph = 86; graph < 97; graph++)
        FOR (unresolved_private_19_8 = 0; unresolved_private_19_8 < 2; unresolved_private_19_8++)
            FRAME;
        END
    END
    unresolved_global_1760 = 0;
    unresolved_global_1764 = 0;
END

PROCESS proc_742(INT param0, INT param1, INT param2, INT param3)
PRIVATE
    INT unresolved_private_20_16;  // offset 16
    INT unresolved_private_20_20;  // offset 20
BEGIN
    x = param0;
    y = param1;
    file = unresolved_global_70532;
    resolution = 10;
    FOR (unresolved_private_20_20 = 0; unresolved_private_20_20 < 4; unresolved_private_20_20++)
        FOR (graph = 270; graph < 274; graph++)
            FOR (unresolved_private_20_16 = 0; unresolved_private_20_16 < 4; unresolved_private_20_16++)
                FRAME;
            END
        END
    END
    FOR (graph = 274; graph < 287; graph++)
        FOR (unresolved_private_20_16 = 0; unresolved_private_20_16 < 2; unresolved_private_20_16++)
            FRAME;
        END
    END
    proc_747(x, y, param2, param3);
END

PROCESS proc_743(INT param0, INT param1, INT param2)
PRIVATE
    INT unresolved_private_21_12;  // offset 12
BEGIN
    x = param0;
    y = param1;
    graph = param2;
    file = unresolved_global_70532;
    resolution = 10;
    z = -256;
    unresolved_private_21_12 = y;
    FOR (y = unresolved_private_21_12; y > unresolved_private_21_12 - 160; y = y - 5)
        FRAME;
    END
END

PROCESS proc_747(INT param0, INT param1, INT param2, INT param3)
PRIVATE
    INT unresolved_private_22_16;  // offset 16
    INT unresolved_private_22_24;  // offset 24
    INT unresolved_private_22_32;  // offset 32
    INT unresolved_private_22_36;  // offset 36
    INT unresolved_private_22_44;  // offset 44
    INT unresolved_private_22_48;  // offset 48
    INT unresolved_private_22_52;  // offset 52
    INT unresolved_private_22_64;  // offset 64
BEGIN
    x = param0;
    y = param1;
    unresolved_private_22_44 = 60;
    resolution = 10;
    file = unresolved_global_70532;
    graph = 100;
    region = 1;
    LOOP
        unresolved_private_22_16 = 0;
        unresolved_private_22_64 = 0;
        unresolved_private_22_32 = unresolved_global_1768;
        unresolved_private_22_36 = unresolved_global_1772;
        graph = param3 + 171;
        FOR (unresolved_private_22_24 = 0; unresolved_private_22_24 < 80; unresolved_private_22_24++)
            IF (x % 160 == 0 && y % 160 == 0)
                IF (x == unresolved_global_1768 && y == unresolved_global_1772)
                    unresolved_private_22_64 = 1;
                END
                IF (x / 160 < 21 && (x / 160 > 0 && ((y - 160) / 160 < 14 && ((y - 160) / 160 > 0 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160] < 1))))
                    IF (unresolved_private_22_44 > rand(0, 99))
                        IF (x >= unresolved_private_22_32)
                            unresolved_private_22_48 = x - unresolved_private_22_32;
                        END
                        IF (x < unresolved_private_22_32)
                            unresolved_private_22_48 = unresolved_private_22_32 - x;
                        END
                        IF (y >= unresolved_private_22_36)
                            unresolved_private_22_52 = y - unresolved_private_22_36;
                        END
                        IF (y < unresolved_private_22_36)
                            unresolved_private_22_52 = unresolved_private_22_36 - y;
                        END
                        IF (param3 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                            IF (unresolved_private_22_36 <= y && unresolved_private_22_52 >= unresolved_private_22_48)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                    param3 = 4;
                                END
                            END
                            IF (unresolved_private_22_36 > y && unresolved_private_22_52 >= unresolved_private_22_48)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                    param3 = 2;
                                END
                            END
                        END
                        IF (param3 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                            IF (unresolved_private_22_32 <= x && unresolved_private_22_48 >= unresolved_private_22_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                    param3 = 3;
                                END
                            END
                            IF (unresolved_private_22_32 > x && unresolved_private_22_48 >= unresolved_private_22_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                    param3 = 1;
                                END
                            END
                        END
                        IF (param3 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                            IF (unresolved_private_22_36 <= y && unresolved_private_22_52 >= unresolved_private_22_48)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                    param3 = 4;
                                END
                            END
                            IF (unresolved_private_22_36 > y && unresolved_private_22_52 >= unresolved_private_22_48)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                    param3 = 2;
                                END
                            END
                        END
                        IF (param3 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                            IF (unresolved_private_22_32 <= x && unresolved_private_22_48 >= unresolved_private_22_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                    param3 = 3;
                                END
                            END
                            IF (unresolved_private_22_32 > x && unresolved_private_22_48 >= unresolved_private_22_52)
                                IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                    param3 = 1;
                                END
                            END
                        END
                    END
                    IF (param3 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_22_36 <= y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                param3 = 4;
                            ELSE
                                param3 = 2;
                            END
                        END
                        IF (unresolved_private_22_36 > y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                param3 = 2;
                            ELSE
                                param3 = 4;
                            END
                        END
                    END
                    IF (param3 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] > 0)
                        IF (unresolved_private_22_32 <= x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                param3 = 3;
                            ELSE
                                param3 = 1;
                            END
                        END
                        IF (unresolved_private_22_32 > x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                param3 = 1;
                            ELSE
                                param3 = 3;
                            END
                        END
                    END
                    IF (param3 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_22_36 <= y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                param3 = 4;
                            ELSE
                                param3 = 2;
                            END
                        END
                        IF (unresolved_private_22_36 > y)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                param3 = 2;
                            ELSE
                                param3 = 4;
                            END
                        END
                    END
                    IF (param3 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] > 0)
                        IF (unresolved_private_22_32 <= x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                param3 = 3;
                            ELSE
                                param3 = 1;
                            END
                        END
                        IF (unresolved_private_22_32 > x)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                param3 = 1;
                            ELSE
                                param3 = 3;
                            END
                        END
                    END
                END
            END
            IF (x % 160 == 0 && y % 160 == 0)
                IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] == 4)
                    param3 = 1;
                END
                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] == 4)
                    param3 = 2;
                END
                IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] == 4)
                    param3 = 3;
                END
                IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] == 4)
                    param3 = 4;
                END
            END
            IF (param3 == 1)
                x = x + 1;
            END
            IF (param3 == 2)
                y = y + 1;
            END
            IF (param3 == 3)
                x = x - 1;
            END
            IF (param3 == 4)
                y = y - 1;
            END
            IF (y > 2480)
                y = 8;
            END
            IF (y < 8)
                y = 2480;
            END
            IF (x > 3280)
                x = -8;
            END
            IF (x < -8)
                x = 3280;
            END
        END
        IF (unresolved_private_22_64 == 1)
            proc_749(x, y, param2, param3);
            BREAK;
        END
        FRAME;
    END
END

PROCESS proc_749(INT param0, INT param1, INT param2, INT param3)
PRIVATE
    INT unresolved_private_23_16;  // offset 16
    INT unresolved_private_23_20;  // offset 20
    INT unresolved_private_23_24;  // offset 24
BEGIN
    x = param0;
    y = param1;
    file = unresolved_global_70532;
    resolution = 10;
    play_wav(unresolved_global_70620, 0);
    unresolved_private_23_24 = 1280;
    IF (unresolved_global_1888[unresolved_global_1784][unresolved_global_1768 / 160][(unresolved_global_1772 - 160) / 160 - 1] == 4)
        IF (param2 == 1)
            x = unresolved_global_1768;
            y = unresolved_global_1772;
        END
        IF (param2 == 2)
            x = unresolved_global_1768 - 160;
            y = unresolved_global_1772;
        END
        IF (param2 == 3)
            x = unresolved_global_1768 + 160;
            y = unresolved_global_1772;
        END
        IF (param2 == 4)
            x = unresolved_global_1768;
            y = unresolved_global_1772;
        END
    END
    IF (unresolved_global_1888[unresolved_global_1784][unresolved_global_1768 / 160][(unresolved_global_1772 - 160) / 160 + 1] == 4)
        IF (param2 == 1)
            x = unresolved_global_1768;
            y = unresolved_global_1772;
        END
        IF (param2 == 2)
            x = unresolved_global_1768 - 160;
            y = unresolved_global_1772;
        END
        IF (param2 == 3)
            x = unresolved_global_1768 + 160;
            y = unresolved_global_1772;
        END
        IF (param2 == 4)
            x = unresolved_global_1768;
            y = unresolved_global_1772;
        END
    END
    IF (unresolved_global_1888[unresolved_global_1784][unresolved_global_1768 / 160 + 1][(unresolved_global_1772 - 160) / 160] == 4)
        IF (param2 == 1)
            x = unresolved_global_1768;
            y = unresolved_global_1772;
        END
        IF (param2 == 2)
            x = unresolved_global_1768;
            y = unresolved_global_1772 - 160;
        END
        IF (param2 == 3)
            x = unresolved_global_1768;
            y = unresolved_global_1772 + 160;
        END
        IF (param2 == 4)
            x = unresolved_global_1768;
            y = unresolved_global_1772;
        END
    END
    IF (unresolved_global_1888[unresolved_global_1784][unresolved_global_1768 / 160 - 1][(unresolved_global_1772 - 160) / 160] == 4)
        IF (param2 == 1)
            x = unresolved_global_1768;
            y = unresolved_global_1772;
        END
        IF (param2 == 2)
            x = unresolved_global_1768;
            y = unresolved_global_1772 - 160;
        END
        IF (param2 == 3)
            x = unresolved_global_1768;
            y = unresolved_global_1772 + 160;
        END
        IF (param2 == 4)
            x = unresolved_global_1768;
            y = unresolved_global_1772;
        END
    END
    IF (param2 == 1)
        unresolved_private_23_20 = 209;
    END
    IF (param2 == 2)
        unresolved_private_23_20 = 187;
    END
    IF (param2 == 3)
        unresolved_private_23_20 = 198;
    END
    IF (param2 == 4)
        unresolved_private_23_20 = 176;
    END
    FOR (unresolved_private_23_16 = 0; unresolved_private_23_16 < 11; unresolved_private_23_16 = unresolved_private_23_16 + 1)
        graph = unresolved_private_23_16 + unresolved_private_23_20;
        FRAME;
    END
    proc_700(x, y, param2, param3, unresolved_private_23_24);
END

PROCESS proc_753()
PRIVATE
    INT unresolved_private_24_0;  // offset 0
BEGIN
    z = -256;
    file = unresolved_global_70532;
    x = 160;
    y = 120;
    IF (unresolved_global_1692 > unresolved_global_1696[unresolved_global_1816])
        unresolved_global_1696[unresolved_global_1816] = unresolved_global_1692;
    END
    play_song(unresolved_global_1852, 0);
    FOR (graph = 524; graph < 535; graph++)
        FOR (unresolved_private_24_0 = 0; unresolved_private_24_0 < 2; unresolved_private_24_0++)
            FRAME;
        END
    END
    REPEAT
        FRAME;
    UNTIL (is_playing_song() == 0);
    signal(TYPE proc_695, 0);
    signal(TYPE proc_701, 0);
    signal(TYPE proc_694, 0);
    signal(TYPE proc_695, 0);
    signal(TYPE proc_693, 0);
    signal(TYPE proc_696, 0);
    signal(TYPE proc_772, 0);
    delete_text(0);
    save("hiscores", UNRESOLVED_GLOBAL_1696);
    proc_687();
END

PROCESS proc_758()
PRIVATE
    INT unresolved_private_25_0;  // offset 0
    INT unresolved_private_25_4;  // offset 4
    INT unresolved_private_25_8;  // offset 8
BEGIN
    z = -256;
    unresolved_private_25_8 = y;
    unresolved_private_25_4 = 1600;
    play_wav(unresolved_global_70584, 0);
    REPEAT
        y = unresolved_private_25_8 - unresolved_private_25_4;
        unresolved_private_25_4 = unresolved_private_25_4 - unresolved_private_25_0;
        unresolved_private_25_0 = unresolved_private_25_0 + 5;
        IF (unresolved_private_25_0 > 60)
            unresolved_private_25_0 = 60;
        END
        FRAME;
    UNTIL (unresolved_private_25_4 <= 80);
    play_wav(unresolved_global_70644, 0);
END

PROCESS proc_744(INT param0, INT param1, INT param2)
PRIVATE
    INT unresolved_private_26_12;  // offset 12
    INT unresolved_private_26_16;  // offset 16
    INT unresolved_private_26_20;  // offset 20
    INT unresolved_private_26_24;  // offset 24
    INT unresolved_private_26_32;  // offset 32
BEGIN
    x = param0;
    y = param1;
    unresolved_private_26_32 = unresolved_global_1776;
    resolution = 10;
    file = unresolved_global_70532;
    graph = 636;
    region = 1;
    unresolved_private_26_12 = param2;
    LOOP
        graph = 78 + unresolved_private_26_24;
        unresolved_private_26_24++;
        IF (unresolved_private_26_24 > 7)
            unresolved_private_26_24 = 0;
        END
        FOR (unresolved_private_26_20 = 0; unresolved_private_26_20 < unresolved_private_26_32; unresolved_private_26_20++)
            unresolved_private_26_16 = rand(1, 2);
            IF (x % 160 == 0 && y % 160 == 0)
                IF (x / 160 < 21 && (x / 160 > 0 && ((y - 160) / 160 < 14 && (y - 160) / 160 > 0)))
                    IF (param2 == 1 || param2 == 3)
                        IF (unresolved_private_26_16 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                            unresolved_private_26_12 = 4;
                        END
                        IF (unresolved_private_26_16 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                            unresolved_private_26_12 = 2;
                        END
                    END
                    IF (param2 == 2 || param2 == 4)
                        IF (unresolved_private_26_16 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                            unresolved_private_26_12 = 1;
                        END
                        IF (unresolved_private_26_16 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                            unresolved_private_26_12 = 3;
                        END
                    END
                    IF (param2 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_26_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                unresolved_private_26_12 = 4;
                            ELSE
                                unresolved_private_26_12 = 2;
                            END
                        END
                        IF (unresolved_private_26_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                unresolved_private_26_12 = 2;
                            ELSE
                                unresolved_private_26_12 = 4;
                            END
                        END
                    END
                    IF (param2 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] > 0)
                        IF (unresolved_private_26_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                unresolved_private_26_12 = 1;
                            ELSE
                                unresolved_private_26_12 = 3;
                            END
                        END
                        IF (unresolved_private_26_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                unresolved_private_26_12 = 3;
                            ELSE
                                unresolved_private_26_12 = 1;
                            END
                        END
                    END
                    IF (param2 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_26_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                unresolved_private_26_12 = 4;
                            ELSE
                                unresolved_private_26_12 = 2;
                            END
                        END
                        IF (unresolved_private_26_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                unresolved_private_26_12 = 2;
                            ELSE
                                unresolved_private_26_12 = 4;
                            END
                        END
                    END
                    IF (param2 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] > 0)
                        IF (unresolved_private_26_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                unresolved_private_26_12 = 1;
                            ELSE
                                unresolved_private_26_12 = 3;
                            END
                        END
                        IF (unresolved_private_26_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                unresolved_private_26_12 = 3;
                            ELSE
                                unresolved_private_26_12 = 1;
                            END
                        END
                    END
                END
                IF (x == unresolved_global_1768 && y == unresolved_global_1772)
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] == 4)
                        unresolved_private_26_12 = 1;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] == 4)
                        unresolved_private_26_12 = 2;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] == 4)
                        unresolved_private_26_12 = 3;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] == 4)
                        unresolved_private_26_12 = 4;
                    END
                END
            END
            param2 = unresolved_private_26_12;
            IF (param2 == 1)
                x = x + 1;
            END
            IF (param2 == 2)
                y = y + 1;
            END
            IF (param2 == 3)
                x = x - 1;
            END
            IF (param2 == 4)
                y = y - 1;
            END
            IF (y > 2480)
                y = 8;
            END
            IF (y < 8)
                y = 2480;
            END
            IF (x > 3280)
                x = -8;
            END
            IF (x < -8)
                x = 3280;
            END
        END
        unresolved_global_1760 = x;
        unresolved_global_1764 = y;
        FRAME;
    END
END

PROCESS proc_752(INT param0, INT param1, INT param2)
PRIVATE
    INT unresolved_private_27_12;  // offset 12
    INT unresolved_private_27_16;  // offset 16
    INT unresolved_private_27_20;  // offset 20
    INT unresolved_private_27_24;  // offset 24
    INT unresolved_private_27_28;  // offset 28
    INT unresolved_private_27_32;  // offset 32
BEGIN
    x = param0;
    y = param1;
    unresolved_private_27_32 = unresolved_global_1776;
    resolution = 10;
    file = unresolved_global_70532;
    graph = 636;
    region = 1;
    z = -255;
    unresolved_private_27_12 = param2;
    FOR (unresolved_private_27_28 = 0; unresolved_private_27_28 < 10000; unresolved_private_27_28 = unresolved_private_27_28 + unresolved_private_27_32)
        graph = 72 + unresolved_private_27_24;
        unresolved_private_27_24++;
        IF (unresolved_private_27_24 > 5)
            unresolved_private_27_24 = 0;
        END
        FOR (unresolved_private_27_20 = 0; unresolved_private_27_20 < unresolved_private_27_32; unresolved_private_27_20++)
            unresolved_private_27_16 = rand(1, 2);
            IF (x % 160 == 0 && y % 160 == 0)
                IF (unresolved_global_69208[x / 160][(y - 160) / 160] == 1)
                    unresolved_global_69208[x / 160][(y - 160) / 160] = 0;
                    unresolved_global_1692 = unresolved_global_1692 + 10;
                END
                IF (x / 160 < 21 && (x / 160 > 0 && ((y - 160) / 160 < 14 && (y - 160) / 160 > 0)))
                    IF (param2 == 1 || param2 == 3)
                        IF (unresolved_private_27_16 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                            unresolved_private_27_12 = 4;
                        END
                        IF (unresolved_private_27_16 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                            unresolved_private_27_12 = 2;
                        END
                    END
                    IF (param2 == 2 || param2 == 4)
                        IF (unresolved_private_27_16 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                            unresolved_private_27_12 = 1;
                        END
                        IF (unresolved_private_27_16 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                            unresolved_private_27_12 = 3;
                        END
                    END
                    IF (param2 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_27_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                unresolved_private_27_12 = 4;
                            ELSE
                                unresolved_private_27_12 = 2;
                            END
                        END
                        IF (unresolved_private_27_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                unresolved_private_27_12 = 2;
                            ELSE
                                unresolved_private_27_12 = 4;
                            END
                        END
                    END
                    IF (param2 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] > 0)
                        IF (unresolved_private_27_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                unresolved_private_27_12 = 1;
                            ELSE
                                unresolved_private_27_12 = 3;
                            END
                        END
                        IF (unresolved_private_27_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                unresolved_private_27_12 = 3;
                            ELSE
                                unresolved_private_27_12 = 1;
                            END
                        END
                    END
                    IF (param2 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_27_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                unresolved_private_27_12 = 4;
                            ELSE
                                unresolved_private_27_12 = 2;
                            END
                        END
                        IF (unresolved_private_27_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                unresolved_private_27_12 = 2;
                            ELSE
                                unresolved_private_27_12 = 4;
                            END
                        END
                    END
                    IF (param2 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] > 0)
                        IF (unresolved_private_27_16 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                unresolved_private_27_12 = 1;
                            ELSE
                                unresolved_private_27_12 = 3;
                            END
                        END
                        IF (unresolved_private_27_16 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                unresolved_private_27_12 = 3;
                            ELSE
                                unresolved_private_27_12 = 1;
                            END
                        END
                    END
                END
                IF (x == unresolved_global_1768 && y == unresolved_global_1772)
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] == 4)
                        unresolved_private_27_12 = 1;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] == 4)
                        unresolved_private_27_12 = 2;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] == 4)
                        unresolved_private_27_12 = 3;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] == 4)
                        unresolved_private_27_12 = 4;
                    END
                END
            END
            param2 = unresolved_private_27_12;
            IF (param2 == 1)
                x = x + 1;
            END
            IF (param2 == 2)
                y = y + 1;
            END
            IF (param2 == 3)
                x = x - 1;
            END
            IF (param2 == 4)
                y = y - 1;
            END
            IF (y > 2480)
                y = 8;
            END
            IF (y < 8)
                y = 2480;
            END
            IF (x > 3280)
                x = -8;
            END
            IF (x < -8)
                x = 3280;
            END
        END
        unresolved_global_1752 = x;
        unresolved_global_1756 = y;
        FRAME;
    END
    unresolved_global_1752 = 0;
    unresolved_global_1756 = 0;
    proc_741(x, y);
END

PROCESS proc_701(INT param0, INT param1, INT param2, INT param3)
PRIVATE
    INT unresolved_private_28_16;  // offset 16
    INT unresolved_private_28_20;  // offset 20
    INT unresolved_private_28_24;  // offset 24
    INT unresolved_private_28_28;  // offset 28
    INT unresolved_private_28_32;  // offset 32
    INT unresolved_private_28_36;  // offset 36
    INT unresolved_private_28_40;  // offset 40
    INT unresolved_private_28_44;  // offset 44
BEGIN
    x = param0;
    y = param1;
    unresolved_private_28_32 = (unresolved_global_1776 * 100 - unresolved_global_1776 * 30) / 100;
    unresolved_private_28_40 = 1;
    resolution = 10;
    file = unresolved_global_70532;
    graph = 636;
    region = 1;
    REPEAT
        IF (unresolved_global_1816 != 2)
            unresolved_private_28_44 = unresolved_private_28_44 + unresolved_private_28_32;
        END
        graph = param3 * 4 + 400 + unresolved_private_28_28;
        unresolved_private_28_28 = unresolved_private_28_28 + unresolved_private_28_40;
        IF (unresolved_private_28_28 > 3)
            unresolved_private_28_28 = 2;
            unresolved_private_28_40 = -1;
        END
        IF (unresolved_private_28_28 < 0)
            unresolved_private_28_28 = 1;
            unresolved_private_28_40 = 1;
        END
        FOR (unresolved_private_28_24 = 0; unresolved_private_28_24 < unresolved_private_28_32; unresolved_private_28_24++)
            unresolved_private_28_20 = rand(1, 2);
            IF (x > unresolved_global_1744 - 60 && (x < unresolved_global_1744 + 60 && (y > unresolved_global_1748 - 60 && y < unresolved_global_1748 + 60)))
                unresolved_private_28_36 = 1;
            END
            IF (x > unresolved_global_1752 - 60 && (x < unresolved_global_1752 + 60 && (y > unresolved_global_1756 - 60 && y < unresolved_global_1756 + 60)))
                unresolved_private_28_36 = 1;
            END
            IF (x % 160 == 0 && y % 160 == 0)
                IF (x / 160 < 21 && (x / 160 > 0 && ((y - 160) / 160 < 14 && (y - 160) / 160 > 0)))
                    IF (param2 == 1 || param2 == 3)
                        IF (unresolved_private_28_20 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                            unresolved_private_28_16 = 4;
                        END
                        IF (unresolved_private_28_20 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                            unresolved_private_28_16 = 2;
                        END
                    END
                    IF (param2 == 2 || param2 == 4)
                        IF (unresolved_private_28_20 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                            unresolved_private_28_16 = 1;
                        END
                        IF (unresolved_private_28_20 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                            unresolved_private_28_16 = 3;
                        END
                    END
                    IF (param2 == 1 && unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_28_20 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                unresolved_private_28_16 = 4;
                            ELSE
                                unresolved_private_28_16 = 2;
                            END
                        END
                        IF (unresolved_private_28_20 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                unresolved_private_28_16 = 2;
                            ELSE
                                unresolved_private_28_16 = 4;
                            END
                        END
                    END
                    IF (param2 == 2 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] > 0)
                        IF (unresolved_private_28_20 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                unresolved_private_28_16 = 1;
                            ELSE
                                unresolved_private_28_16 = 3;
                            END
                        END
                        IF (unresolved_private_28_20 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                unresolved_private_28_16 = 3;
                            ELSE
                                unresolved_private_28_16 = 1;
                            END
                        END
                    END
                    IF (param2 == 3 && unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] > 0)
                        IF (unresolved_private_28_20 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] < 1)
                                unresolved_private_28_16 = 4;
                            ELSE
                                unresolved_private_28_16 = 2;
                            END
                        END
                        IF (unresolved_private_28_20 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] < 1)
                                unresolved_private_28_16 = 2;
                            ELSE
                                unresolved_private_28_16 = 4;
                            END
                        END
                    END
                    IF (param2 == 4 && unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] > 0)
                        IF (unresolved_private_28_20 == 1)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] < 1)
                                unresolved_private_28_16 = 1;
                            ELSE
                                unresolved_private_28_16 = 3;
                            END
                        END
                        IF (unresolved_private_28_20 == 2)
                            IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] < 1)
                                unresolved_private_28_16 = 3;
                            ELSE
                                unresolved_private_28_16 = 1;
                            END
                        END
                    END
                END
                IF (x == unresolved_global_1768 && y == unresolved_global_1772)
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 + 1][(y - 160) / 160] == 4)
                        unresolved_private_28_16 = 1;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 + 1] == 4)
                        unresolved_private_28_16 = 2;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160 - 1][(y - 160) / 160] == 4)
                        unresolved_private_28_16 = 3;
                    END
                    IF (unresolved_global_1888[unresolved_global_1784][x / 160][(y - 160) / 160 - 1] == 4)
                        unresolved_private_28_16 = 4;
                    END
                END
            END
            param2 = unresolved_private_28_16;
            IF (param2 == 1)
                x = x + 1;
            END
            IF (param2 == 2)
                y = y + 1;
            END
            IF (param2 == 3)
                x = x - 1;
            END
            IF (param2 == 4)
                y = y - 1;
            END
            IF (y > 2480)
                y = 8;
            END
            IF (y < 8)
                y = 2480;
            END
            IF (x > 3280)
                x = -8;
            END
            IF (x < -8)
                x = 3280;
            END
        END
        FRAME;
        IF (unresolved_private_28_36 == 1)
            proc_743(x, y, param3 + 464);
            unresolved_global_1692 = unresolved_global_1692 + (param3 + 1) * 500;
            play_wav(unresolved_global_70600, 0);
            IF (unresolved_global_1816 == 2)
                unresolved_global_1812 = unresolved_global_1812 - 1;
                IF (unresolved_global_1812 == 0)
                    proc_708();
                END
            END
            BREAK;
        END
    UNTIL (unresolved_private_28_44 > 7000);
    IF (unresolved_private_28_36 == 0)
        proc_741(x, y);
    END
END

PROCESS proc_771(INT param0, INT param1, INT param2)
PRIVATE
    INT unresolved_private_29_12;  // offset 12
BEGIN
    x = param0;
    y = param1;
    graph = param2;
    file = unresolved_global_70532;
    resolution = 10;
    unresolved_private_29_12 = 5;
    z = -512;
    REPEAT
        y = y + unresolved_private_29_12;
        IF (y > -160)
            unresolved_private_29_12 = unresolved_private_29_12 + unresolved_private_29_12 + 2;
        END
        IF (unresolved_private_29_12 > 160)
            unresolved_private_29_12 = 160;
        END
        FRAME;
    UNTIL (y >= 1200);
    y = 1200;
    unresolved_private_29_12 = -80;
    REPEAT
        y = y + unresolved_private_29_12;
        unresolved_private_29_12 = unresolved_private_29_12 + 10;
        IF (y >= 1200)
            y = 1200;
            unresolved_private_29_12 = unresolved_private_29_12 / 2;
            IF (unresolved_private_29_12 < 20)
                unresolved_private_29_12 = 0;
            END
            unresolved_private_29_12 = -unresolved_private_29_12;
        END
        FRAME;
    UNTIL (unresolved_private_29_12 == 0 && y == 1200);
    LOOP
        FRAME;
    END
END

PROCESS proc_773(INT param0)
BEGIN
    file = unresolved_global_70532;
    graph = 507;
    x = 160;
    y = 120;
    IF (param0 == 0)
        FOR (alpha = 0; alpha < 256; alpha = alpha + 16)
            FRAME;
        END
    END
    graph = 540;
    put_screen(unresolved_global_70532, 507);
    x = 160;
    y = 110;
    REPEAT
        IF (key(72) == 0 && (key(80) == 0 && (key(56) == 0 && (key(45) == 0 && key(44) == 0))))
            unresolved_global_1800 = 0;
        END
        IF (key(72) && unresolved_global_1800 == 0)
            graph = graph - 1;
            unresolved_global_1800 = 1;
            play_wav(unresolved_global_70660, 0);
        END
        IF (key(80) && unresolved_global_1800 == 0)
            graph = graph + 1;
            unresolved_global_1800 = 1;
            play_wav(unresolved_global_70660, 0);
        END
        IF (graph > 543)
            graph = 540;
        END
        IF (graph < 540)
            graph = 543;
        END
        delete_text(0);
        IF (graph == 540)
            write(0, 160, 180, 1, "Help Ruck-Man collect all the pellets in the");
            write(0, 160, 190, 1, "maze to progress to the next level. To help");
            write(0, 160, 200, 1, "you on your quest there are 5 all new");
            write(0, 160, 210, 1, "power-ups. Use them wisely.");
        END
        IF (graph == 541)
            write(0, 160, 180, 1, "In Fruit Frenzy the aim is eat all the fruit");
            write(0, 160, 190, 1, "and other items wandering around the maze.");
            write(0, 160, 200, 1, "Each new level has increasing numbers of");
            write(0, 160, 210, 1, "items you have to catch and introduces");
            write(0, 160, 220, 1, "new higher scoring items.");
        END
        IF (graph == 542)
            write(0, 160, 170, 1, "This mode takes place on a single maze of");
            write(0, 160, 180, 1, "your choice. The game starts with one trail");
            write(0, 160, 190, 1, "of 25 pellets, collect all these and another");
            write(0, 160, 200, 1, "trail appears. Each time you clear a trail");
            write(0, 160, 210, 1, "the game speeds up a little bit and gets more");
            write(0, 160, 220, 1, "difficult. A power pellet will appear in");
            write(0, 160, 230, 1, "every other trail to help you a little.");
        END
        IF (graph == 543)
            write(0, 160, 190, 1, "The classic game you know and love. Basically");
            write(0, 160, 200, 1, "the same as standard mode but without any of");
            write(0, 160, 210, 1, "the new power-ups.");
        END
        FRAME;
        IF (unresolved_global_1800 == 0)
            IF (key(44) || key(45))
                graph = 0;
                BREAK;
            END
        END
    UNTIL (key(56) && unresolved_global_1800 == 0);
    delete_text(0);
    play_wav(unresolved_global_70640, 0);
    unresolved_global_1800 = 1;
    IF (graph == 540)
        unresolved_global_1816 = 1;
    END
    IF (graph == 541)
        unresolved_global_1816 = 2;
    END
    IF (graph == 542)
        unresolved_global_1816 = 3;
    END
    IF (graph == 543)
        unresolved_global_1816 = 4;
    END
    IF (graph != 0)
        proc_778();
    END
    IF (graph == 0)
        proc_687();
    END
END

PROCESS proc_774()
PRIVATE
    INT unresolved_private_31_0;  // offset 0
    INT unresolved_private_31_4;  // offset 4
BEGIN
    file = unresolved_global_70532;
    graph = 501;
    x = 160;
    y = 120;
    FOR (alpha = 0; alpha < 256; alpha = alpha + 16)
        FRAME;
    END
    put_screen(unresolved_global_70532, 501);
    graph = 498;
    REPEAT
        IF (key(75) == 0 && (key(77) == 0 && (key(72) == 0 && (key(80) == 0 && key(56) == 0))))
            unresolved_global_1800 = 0;
        END
        IF (unresolved_global_1800 == 0)
            IF (key(72))
                unresolved_private_31_0 = unresolved_private_31_0 - 1;
                unresolved_global_1800 = 1;
                play_wav(unresolved_global_70660, 0);
            END
            IF (key(80))
                unresolved_private_31_0 = unresolved_private_31_0 + 1;
                unresolved_global_1800 = 1;
                play_wav(unresolved_global_70660, 0);
            END
            IF (key(77))
                IF (unresolved_private_31_0 == 0)
                    unresolved_global_1864 = unresolved_global_1864 + 1;
                END
                IF (unresolved_private_31_0 == 1)
                    unresolved_global_1868 = unresolved_global_1868 + 1;
                END
                IF (unresolved_private_31_0 == 2)
                    unresolved_global_1876 = unresolved_global_1876 + 5;
                END
                IF (unresolved_private_31_0 == 3)
                    unresolved_global_1872 = unresolved_global_1872 + 5;
                END
                IF (unresolved_private_31_0 == 4)
                    unresolved_global_1880 = unresolved_global_1880 + 1;
                END
                IF (unresolved_private_31_0 == 5)
                    unresolved_global_1884 = unresolved_global_1884 + 1;
                END
                unresolved_global_1800 = 1;
                play_wav(unresolved_global_70660, 0);
            END
            IF (key(75))
                IF (unresolved_private_31_0 == 0)
                    unresolved_global_1864 = unresolved_global_1864 - 1;
                END
                IF (unresolved_private_31_0 == 1)
                    unresolved_global_1868 = unresolved_global_1868 - 1;
                END
                IF (unresolved_private_31_0 == 2)
                    unresolved_global_1876 = unresolved_global_1876 - 5;
                END
                IF (unresolved_private_31_0 == 3)
                    unresolved_global_1872 = unresolved_global_1872 - 5;
                END
                IF (unresolved_private_31_0 == 4)
                    unresolved_global_1880 = unresolved_global_1880 - 1;
                END
                IF (unresolved_private_31_0 == 5)
                    unresolved_global_1884 = unresolved_global_1884 - 1;
                END
                unresolved_global_1800 = 1;
                play_wav(unresolved_global_70660, 0);
            END
            IF (unresolved_global_1864 > 1)
                unresolved_global_1864 = 0;
            END
            IF (unresolved_global_1864 < 0)
                unresolved_global_1864 = 1;
            END
            IF (unresolved_global_1868 > 1)
                unresolved_global_1868 = 0;
            END
            IF (unresolved_global_1868 < 0)
                unresolved_global_1868 = 1;
            END
            IF (unresolved_global_1876 > 100)
                unresolved_global_1876 = 0;
            END
            IF (unresolved_global_1876 < 0)
                unresolved_global_1876 = 100;
            END
            IF (unresolved_global_1872 > 100)
                unresolved_global_1872 = 0;
            END
            IF (unresolved_global_1872 < 0)
                unresolved_global_1872 = 100;
            END
            IF (unresolved_global_1880 > 6)
                unresolved_global_1880 = 1;
            END
            IF (unresolved_global_1880 < 1)
                unresolved_global_1880 = 6;
            END
            IF (unresolved_global_1884 > 1)
                unresolved_global_1884 = 0;
            END
            IF (unresolved_global_1884 < 0)
                unresolved_global_1884 = 1;
            END
        END
        IF (unresolved_private_31_0 > 6)
            unresolved_private_31_0 = 0;
        END
        IF (unresolved_private_31_0 < 0)
            unresolved_private_31_0 = 6;
        END
        delete_text(0);
        write(unresolved_global_1820, 160, 10, 1, "Options");
        IF (unresolved_private_31_0 == 0)
            unresolved_private_31_4 = unresolved_global_1828;
        ELSE
            unresolved_private_31_4 = unresolved_global_1824;
        END
        write(unresolved_private_31_4, 160, 60, 5, "Maze Theme:");
        IF (unresolved_global_1864 == 0)
            write(unresolved_private_31_4, 170, 60, 3, "Sequential");
        ELSE
            write(unresolved_private_31_4, 170, 60, 3, "Random");
        END
        IF (unresolved_private_31_0 == 1)
            unresolved_private_31_4 = unresolved_global_1828;
        ELSE
            unresolved_private_31_4 = unresolved_global_1824;
        END
        write(unresolved_private_31_4, 160, 80, 5, "Maze Order:");
        IF (unresolved_global_1868 == 0)
            write(unresolved_private_31_4, 170, 80, 3, "Sequential");
        ELSE
            write(unresolved_private_31_4, 170, 80, 3, "Random");
        END
        IF (unresolved_private_31_0 == 2)
            unresolved_private_31_4 = unresolved_global_1828;
        ELSE
            unresolved_private_31_4 = unresolved_global_1824;
        END
        write(unresolved_private_31_4, 160, 100, 5, "Music Volume:");
        write(unresolved_private_31_4, 170, 100, 3, unresolved_global_1876);
        IF (unresolved_private_31_0 == 3)
            unresolved_private_31_4 = unresolved_global_1828;
        ELSE
            unresolved_private_31_4 = unresolved_global_1824;
        END
        write(unresolved_private_31_4, 160, 120, 5, "Sound Volume:");
        write(unresolved_private_31_4, 170, 120, 3, unresolved_global_1872);
        IF (unresolved_private_31_0 == 4)
            unresolved_private_31_4 = unresolved_global_1828;
        ELSE
            unresolved_private_31_4 = unresolved_global_1824;
        END
        write(unresolved_private_31_4, 160, 140, 5, "Lives (1-6):");
        write(unresolved_private_31_4, 170, 140, 3, unresolved_global_1880);
        IF (unresolved_private_31_0 == 5)
            unresolved_private_31_4 = unresolved_global_1828;
        ELSE
            unresolved_private_31_4 = unresolved_global_1824;
        END
        write(unresolved_private_31_4, 160, 160, 5, "Control Mode:");
        IF (unresolved_global_1884 == 0)
            write(unresolved_private_31_4, 170, 160, 3, "Normal");
        ELSE
            write(unresolved_private_31_4, 170, 160, 3, "Memorize");
        END
        IF (unresolved_private_31_0 == 6)
            unresolved_private_31_4 = unresolved_global_1828;
        ELSE
            unresolved_private_31_4 = unresolved_global_1824;
        END
        write(unresolved_private_31_4, 160, 200, 1, "Title Screen");
        FRAME;
    UNTIL (key(56) && (unresolved_global_1800 == 0 && unresolved_private_31_0 == 6));
    save("options", UNRESOLVED_GLOBAL_1864);
    unresolved_global_1800 = 1;
    delete_text(0);
    proc_687();
    play_wav(unresolved_global_70640, 0);
END

PROCESS proc_775()
BEGIN
    file = unresolved_global_70532;
    graph = 501;
    x = 160;
    y = 120;
    FOR (alpha = 0; alpha < 256; alpha = alpha + 16)
        FRAME;
    END
    put_screen(unresolved_global_70532, 501);
    graph = 498;
    write(unresolved_global_1820, 115, 5, 1, "Standard");
    write(unresolved_global_1820, 115, 30, 1, unresolved_global_1700);
    write(unresolved_global_1820, 145, 65, 1, "Fruit Frenzy");
    write(unresolved_global_1820, 145, 90, 1, unresolved_global_1704);
    write(unresolved_global_1820, 175, 125, 1, "Panic");
    write(unresolved_global_1820, 175, 150, 1, unresolved_global_1708);
    write(unresolved_global_1820, 205, 185, 1, "Classic");
    write(unresolved_global_1820, 205, 210, 1, unresolved_global_1712);
    REPEAT
        IF (key(72) == 0 && (key(80) == 0 && key(56) == 0))
            unresolved_global_1800 = 0;
        END
        FRAME;
    UNTIL (key(56) && unresolved_global_1800 == 0);
    unresolved_global_1800 = 1;
    delete_text(0);
    proc_687();
END

PROCESS proc_776(INT param0)
PRIVATE
    INT unresolved_private_33_4;  // offset 4
    INT unresolved_private_33_8;  // offset 8
BEGIN
    file = unresolved_global_70532;
    graph = 504;
    x = 160;
    y = 120;
    alpha = 0;
    unresolved_private_33_8 = 16;
    REPEAT
        IF (key(72) == 0 && (key(80) == 0 && (key(56) == 0 && (key(75) == 0 && key(77) == 0))))
            unresolved_global_1800 = 0;
        END
        alpha = alpha + unresolved_private_33_8;
        IF (alpha > 256)
            alpha = 256;
            unresolved_private_33_8 = 0;
        END
        IF (param0 == 0)
            IF (alpha == 256)
                unresolved_private_33_4++;
            END
            IF (unresolved_private_33_4 > 120 && graph < 506)
                unresolved_private_33_8 = -16;
            END
            IF (unresolved_private_33_4 > 240 && graph == 506)
                unresolved_private_33_8 = -16;
            END
            IF (alpha < 0)
                alpha = 0;
                unresolved_private_33_8 = 16;
                graph++;
                unresolved_private_33_4 = 0;
            END
        END
        IF (param0 == 1)
            IF (key(75) && unresolved_global_1800 == 0)
                graph = graph - 1;
                unresolved_global_1800 = 1;
                unresolved_private_33_4 = 0;
            END
            IF (key(77) && unresolved_global_1800 == 0)
                graph = graph + 1;
                unresolved_global_1800 = 1;
                unresolved_private_33_4 = 0;
            END
        END
        delete_text(0);
        IF (param0 == 1 && graph == 504)
            write(0, 160, 200, 1, "Left/Right: Navigate - B: Exit");
        END
        IF (graph > 506)
            graph = 504;
        END
        IF (graph < 504)
            graph = 506;
        END
        FRAME;
    UNTIL (key(56) && unresolved_global_1800 == 0);
    delete_text(0);
    REPEAT
        alpha = alpha - 16;
        FRAME;
    UNTIL (alpha < 0);
    unresolved_global_1800 = 1;
    proc_687();
    RETURN;
END

PROCESS proc_778()
BEGIN
    file = unresolved_global_70532;
    x = 160;
    y = 135;
    graph = 544;
    REPEAT
        IF (key(72) == 0 && (key(80) == 0 && (key(56) == 0 && (key(45) == 0 && key(44) == 0))))
            unresolved_global_1800 = 0;
        END
        IF (key(72) && unresolved_global_1800 == 0)
            graph = graph - 1;
            unresolved_global_1800 = 1;
            play_wav(unresolved_global_70660, 0);
        END
        IF (key(80) && unresolved_global_1800 == 0)
            graph = graph + 1;
            unresolved_global_1800 = 1;
            play_wav(unresolved_global_70660, 0);
        END
        IF (graph > 546)
            graph = 544;
        END
        IF (graph < 544)
            graph = 546;
        END
        delete_text(0);
        IF (graph == 544)
            write(0, 160, 200, 1, "Easy mode begins on stage 1");
        END
        IF (graph == 545)
            write(0, 160, 200, 1, "Medium mode begins on stage 5");
        END
        IF (graph == 546)
            write(0, 160, 200, 1, "Hard mode begins on stage 10");
        END
        FRAME;
        IF (unresolved_global_1800 == 0)
            IF (key(44) || key(45))
                graph = 0;
                BREAK;
            END
        END
    UNTIL (key(56) && unresolved_global_1800 == 0);
    play_wav(unresolved_global_70640, 0);
    delete_text(0);
    unresolved_global_1692 = 0;
    unresolved_global_1688 = unresolved_global_1880;
    unresolved_global_1800 = 1;
    IF (graph == 544)
        unresolved_global_1808 = 1;
    END
    IF (graph == 545)
        unresolved_global_1808 = 5;
    END
    IF (graph == 546)
        unresolved_global_1808 = 10;
    END
    IF (graph != 0)
        IF (unresolved_global_1816 != 3)
            proc_688();
            proc_779();
        END
        IF (unresolved_global_1816 == 3)
            proc_780();
        END
    END
    IF (graph == 0)
        proc_773(1);
    END
END

FUNCTION proc_779()
BEGIN
    set_text_color(rgb(255, 100, 0));
    write(0, 22, -1, 1, "LIVES");
    set_text_color(rgb(255, 150, 80));
    write_int(0, 22, 7, 1, &unresolved_global_1688);
    set_text_color(rgb(255, 255, 0));
    write(0, 75, -1, 1, "STAGE");
    set_text_color(rgb(255, 255, 150));
    write_int(0, 75, 7, 1, &unresolved_global_1808);
    set_text_color(rgb(100, 255, 0));
    write(0, 128, -1, 1, "LEFT");
    set_text_color(rgb(150, 255, 100));
    write_int(0, 128, 7, 1, &unresolved_global_1812);
    set_text_color(rgb(0, 220, 255));
    write(0, 200, -1, 1, "SCORE");
    set_text_color(rgb(150, 255, 255));
    write_int(0, 200, 7, 1, &unresolved_global_1692);
    set_text_color(rgb(255, 0, 255));
    write(0, 280, -1, 1, "HI-SCORE");
    set_text_color(rgb(255, 150, 255));
    write_int(0, 280, 7, 1, &unresolved_global_1696[unresolved_global_1816]);
    proc_772();
END

PROCESS proc_780()
PRIVATE
    INT unresolved_private_36_0;  // offset 0
    INT unresolved_private_36_4;  // offset 4
BEGIN
    graph = 547;
    file = unresolved_global_70532;
    x = 160;
    y = 120;
    write(0, 190, 70, 5, "Maze number:");
    write_int(0, 200, 70, 3, &unresolved_global_1784);
    write(0, 160, 180, 1, "Left/Right to select the maze of your choice");
    write(0, 160, 190, 1, "Press 'B' to start.");
    REPEAT
        IF (key(75) == 0 && (key(77) == 0 && key(56) == 0))
            unresolved_global_1800 = 0;
        END
        IF (key(75) && unresolved_global_1800 == 0)
            unresolved_global_1784 = unresolved_global_1784 - 1;
            unresolved_global_1800 = 1;
            play_wav(unresolved_global_70660, 0);
        END
        IF (key(77) && unresolved_global_1800 == 0)
            unresolved_global_1784 = unresolved_global_1784 + 1;
            unresolved_global_1800 = 1;
            play_wav(unresolved_global_70660, 0);
        END
        IF (unresolved_global_1784 > 19)
            unresolved_global_1784 = 0;
        END
        IF (unresolved_global_1784 < 0)
            unresolved_global_1784 = 19;
        END
        map_clear(unresolved_global_70532, 547, 0);
        FOR (unresolved_private_36_0 = 0; unresolved_private_36_0 < 21; unresolved_private_36_0++)
            FOR (unresolved_private_36_4 = 0; unresolved_private_36_4 < 15; unresolved_private_36_4++)
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_36_0][unresolved_private_36_4] == 1 || unresolved_global_1888[unresolved_global_1784][unresolved_private_36_0][unresolved_private_36_4] == 2)
                    map_put(unresolved_global_70532, 547, 548, unresolved_private_36_0 * 5 + 3, unresolved_private_36_4 * 5 + 3);
                END
            END
        END
        FOR (unresolved_private_36_0 = 0; unresolved_private_36_0 < 21; unresolved_private_36_0++)
            FOR (unresolved_private_36_4 = 0; unresolved_private_36_4 < 15; unresolved_private_36_4++)
                IF (unresolved_global_1888[unresolved_global_1784][unresolved_private_36_0][unresolved_private_36_4] == 1 || unresolved_global_1888[unresolved_global_1784][unresolved_private_36_0][unresolved_private_36_4] == 2)
                    map_put(unresolved_global_70532, 547, 549, unresolved_private_36_0 * 5 + 3, unresolved_private_36_4 * 5 + 3);
                END
            END
        END
        FRAME;
        IF (key(44) || key(45))
            graph = 0;
            BREAK;
        END
    UNTIL (key(56) && unresolved_global_1800 == 0);
    unresolved_global_1800 = 1;
    play_wav(unresolved_global_70640, 0);
    IF (graph != 0)
        delete_text(0);
        proc_688();
        proc_779();
    END
    IF (graph == 0)
        proc_778();
    END
END

PROCESS proc_772()
PRIVATE
    INT unresolved_private_37_0[10];  // offset 0
    INT unresolved_private_37_44;  // offset 44
    INT unresolved_private_37_48;  // offset 48
    INT unresolved_private_37_52;  // offset 52
    INT unresolved_private_37_56;  // offset 56
BEGIN
    priority = 20;
    LOOP
        IF (exists(TYPE proc_688) == 0 && (exists(TYPE proc_689) == 0 && (exists(TYPE proc_708) == 0 && (exists(TYPE proc_753) == 0 && (exists(TYPE proc_736) == 0 && exists(TYPE proc_697) == 0)))))
            IF (key(28) == 0)
                unresolved_private_37_48 = 0;
            END
            IF (key(28) && (unresolved_private_37_48 == 0 && unresolved_private_37_44 == 0))
                unresolved_private_37_48 = 1;
                unresolved_private_37_44 = 1;
                pause_song();
                unresolved_private_37_52 = write(unresolved_global_1820, 160, 180, 1, "Game Paused.");
                set_text_color(rgb(255, 255, 255));
                unresolved_private_37_56 = write(unresolved_global_1828, 160, 216, 1, "Start: Continue    Select: Quit");
                FOR (x = 0; x < 9; x++)
                    unresolved_private_37_0[x] = timer[x];
                END
                signal(TYPE proc_695, 3);
                signal(TYPE proc_699, 3);
                signal(TYPE proc_700, 3);
                signal(TYPE proc_737, 3);
                signal(TYPE proc_747, 3);
                signal(TYPE proc_749, 3);
                signal(TYPE proc_702, 3);
                signal(TYPE proc_752, 3);
                signal(TYPE proc_740, 3);
                signal(TYPE proc_744, 3);
                signal(TYPE proc_735, 3);
                signal(TYPE proc_742, 3);
                signal(TYPE proc_738, 3);
                signal(TYPE proc_758, 3);
                signal(TYPE proc_739, 3);
                signal(TYPE proc_743, 3);
                signal(TYPE proc_701, 3);
                signal(TYPE proc_694, 3);
                signal(TYPE proc_696, 3);
                signal(TYPE proc_693, 3);
            END
            IF (key(28) && (unresolved_private_37_48 == 0 && unresolved_private_37_44 == 1))
                unresolved_private_37_48 = 1;
                unresolved_private_37_44 = 0;
                delete_text(unresolved_private_37_52);
                delete_text(unresolved_private_37_56);
                resume_song();
                FOR (x = 0; x < 9; x++)
                    timer[x] = unresolved_private_37_0[x];
                END
                signal(TYPE proc_695, 1);
                signal(TYPE proc_699, 1);
                signal(TYPE proc_700, 1);
                signal(TYPE proc_737, 1);
                signal(TYPE proc_747, 1);
                signal(TYPE proc_749, 1);
                signal(TYPE proc_702, 1);
                signal(TYPE proc_752, 1);
                signal(TYPE proc_740, 1);
                signal(TYPE proc_744, 1);
                signal(TYPE proc_735, 1);
                signal(TYPE proc_742, 1);
                signal(TYPE proc_738, 1);
                signal(TYPE proc_758, 1);
                signal(TYPE proc_739, 1);
                signal(TYPE proc_743, 1);
                signal(TYPE proc_701, 1);
                signal(TYPE proc_694, 1);
                signal(TYPE proc_696, 1);
                signal(TYPE proc_693, 1);
            END
            IF (unresolved_private_37_44 == 1 && key(57))
                unresolved_private_37_44 = 0;
                unresolved_global_1688 = 1;
                proc_736();
            END
        END
        FRAME;
    END
END

