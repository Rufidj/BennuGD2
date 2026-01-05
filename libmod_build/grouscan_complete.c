// ============================================================================
// Phase C: grouscan() - Complete floor/ceiling rendering (from engine.cpp:3950-4171)
// ============================================================================

static void grouscan(int32_t dax1, int32_t dax2, int32_t sectnum, char dastat)
{
    int32_t i, l, x, y, dx, dy, wx, wy, y1, y2, daz;
    int32_t daslope, dasqr;
    int32_t shoffs, m1, m2, shy1, shy2;
    intptr_t *mptr1, *mptr2, j;
    
    BUILD_Sector *sec = &g_map->sectors[sectnum];
    BUILD_Wall *wal;
    
    // Setup - get sector info based on ceiling (0) or floor (1)
    if (dastat == 0) {
        // Ceiling
        if (globalposz <= getceilzofslope(sectnum, globalposx, globalposy))
            return;  // Back-face culling
        
        globalorientation = sec->ceilingstat;
        globalpicnum = sec->ceilingpicnum;
        globalshade = sec->ceilingshade;
        globalpal = sec->ceilingpal;
        daslope = sec->ceilingheinum;
        daz = sec->ceilingz;
    } else {
        // Floor
        if (globalposz >= getflorzofslope(sectnum, globalposx, globalposy))
            return;  // Back-face culling
        
        globalorientation = sec->floorstat;
        globalpicnum = sec->floorpicnum;
        globalshade = sec->floorshade;
        globalpal = sec->floorpal;
        daslope = sec->floorheinum;
        daz = sec->floorz;
    }
    
    // Check if tile exists
    if ((unsigned)globalpicnum >= (unsigned)MAXTILES) return;
    if (tilesizx[globalpicnum] <= 0 || tilesizy[globalpicnum] <= 0) return;
    
    // Load tile
    if (waloff[globalpicnum] == 0) loadtile(globalpicnum);
    globalbufplc = waloff[globalpicnum];
    
    // Get first wall for slope calculations
    wal = &g_map->walls[sec->wallptr];
    BUILD_Wall *wal2 = &g_map->walls[wal->point2];
    wx = wal2->x - wal->x;
    wy = wal2->y - wal->y;
    dasqr = krecipasm(nsqrtasm(uhypsq(wx, wy)));
    i = mulscale21(daslope, dasqr);
    wx *= i;
    wy *= i;
    
    // Setup perspective globals
    globalx = -mulscale19(singlobalang, xdimenrecip);
    globaly = mulscale19(cosglobalang, xdimenrecip);
    globalx1 = (globalposx << 8);
    globaly1 = -(globalposy << 8);
    i = (dax1 - halfxdimen) * xdimenrecip;
    globalx2 = mulscale16(cosglobalang << 4, viewingrangerecip) - mulscale27(singlobalang, i);
    globaly2 = mulscale16(singlobalang << 4, viewingrangerecip) + mulscale27(cosglobalang, i);
    globalzd = (int64_t)xdimscale << 9;
    globalzx = -dmulscale(wx, globaly2, -wy, globalx2, 17) + mulscale10(1 - globalhoriz, globalzd);
    globalz = -dmulscale(wx, globaly, -wy, globalx, 25);
    
    // Handle relative alignment
    if (globalorientation & 64) {
        dx = mulscale14(wal2->x - wal->x, dasqr);
        dy = mulscale14(wal2->y - wal->y, dasqr);
        i = nsqrtasm(daslope * daslope + 16777216);
        
        int32_t tempx = globalx, tempy = globaly;
        globalx = dmulscale(tempx, dx, tempy, dy, 16);
        globaly = mulscale12(dmulscale(-tempy, dx, tempx, dy, 16), i);
        
        x = ((wal->x - globalposx) << 8);
        y = ((wal->y - globalposy) << 8);
        globalx1 = dmulscale(-x, dx, -y, dy, 16);
        globaly1 = mulscale12(dmulscale(-y, dx, x, dy, 16), i);
        
        tempx = globalx2; tempy = globaly2;
        globalx2 = dmulscale(tempx, dx, tempy, dy, 16);
        globaly2 = mulscale12(dmulscale(-tempy, dx, tempx, dy, 16), i);
    }
    
    // Handle flipping
    if (globalorientation & 0x4) {
        i = globalx; globalx = -globaly; globaly = -i;
        i = globalx1; globalx1 = globaly1; globaly1 = i;
        i = globalx2; globalx2 = -globaly2; globaly2 = -i;
    }
    if (globalorientation & 0x10) { globalx1 = -globalx1; globalx2 = -globalx2; globalx = -globalx; }
    if (globalorientation & 0x20) { globaly1 = -globaly1; globaly2 = -globaly2; globaly = -globaly; }
    
    daz = dmulscale(wx, globalposy - wal->y, -wy, globalposx - wal->x, 9) + ((daz - globalposz) << 8);
    globalx2 = mulscale20(globalx2, daz);
    globalx = mulscale28(globalx, daz);
    globaly2 = mulscale20(globaly2, -daz);
    globaly = mulscale28(globaly, -daz);
    
    // Texture coordinate setup
    i = 8 - (picsiz[globalpicnum] & 15);
    int jj = 8 - (picsiz[globalpicnum] >> 4);
    if (globalorientation & 8) { i++; jj++; }
    
    globalx1 <<= (i + 12);
    globaly1 <<= (jj + 12);
    
    if (i >= 0) { globalx2 <<= i; globalx <<= i; }
    else { globalx2 >>= -i; globalx >>= -i; }
    
    if (jj >= 0) { globaly2 <<= jj; globaly <<= jj; }
    else { globaly2 >>= -jj; globaly >>= -jj; }
    
    // Apply panning
    if (dastat == 0) {
        globalx1 += (uint32_t)sec->ceilingxpanning << 24;
        globaly1 += (uint32_t)sec->ceilingypanning << 24;
    } else {
        globalx1 += (uint32_t)sec->floorxpanning << 24;
        globaly1 += (uint32_t)sec->floorypanning << 24;
    }
    
    // Visibility calculation
    asm1 = -(globalzd >> (16 - 3));  // BITSOFPRECISION = 3
    
    int32_t vis = (sec->visibility != 0) ? 
        mulscale4(globalvisibility, (uint8_t)(sec->visibility + 16)) : globalvisibility;
    globvis = ((((int64_t)(vis * (int64_t)daz)) >> 13) * xdimscale) >> 16;
    
    j = (intptr_t)&g_palookup[globalpal * 256];
    
    // Setup texture info for slopevlin
    glogx = picsiz[globalpicnum] & 15;
    glogy = picsiz[globalpicnum] >> 4;
    gbuf = (char *)globalbufplc;
    
    l = (globalzd >> 16);
    
    int32_t shinc = mulscale16(globalz, xdimenscale);
    
    shoffs = (shinc > 0) ? (4 << 15) : ((16380 - ydimen) << 15);
    y1 = (dastat == 0) ? umost[dax1] : ((umost[dax1] > dplc[dax1]) ? umost[dax1] : dplc[dax1]);
    
    m1 = mulscale16(y1, globalzd) + (globalzx >> 6);
    m1 += klabs((int32_t)(globalzd >> 16));
    m2 = m1 + l;
    shy1 = y1 + (shoffs >> 15);
    
    if ((unsigned)shy1 >= SLOPALOOKUPSIZ - 1) return;
    
    mptr1 = &slopalookup[shy1];
    mptr2 = mptr1 + 1;
    
    // Main rendering loop
    for (x = dax1; x <= dax2; x++) {
        if (dastat == 0) {
            y1 = umost[x];
            y2 = (dmost[x] < uplc[x]) ? dmost[x] : uplc[x];
            y2--;
        } else {
            y1 = (umost[x] > dplc[x]) ? umost[x] : dplc[x];
            y2 = dmost[x] - 1;
        }
        
        if (y1 <= y2) {
            shy1 = y1 + (shoffs >> 15);
            shy2 = y2 + (shoffs >> 15);
            
            if ((unsigned)shy1 >= SLOPALOOKUPSIZ) goto next_most;
            if ((unsigned)shy2 >= SLOPALOOKUPSIZ) goto next_most;
            
            intptr_t *nptr1 = &slopalookup[shy1];
            intptr_t *nptr2 = &slopalookup[shy2];
            
            // Update slopalookup with shading
            while (nptr1 <= mptr1) {
                *mptr1-- = j + getpalookupsh(mulscale24(krecipasm(m1), globvis));
                m1 -= l;
            }
            while (nptr2 >= mptr2) {
                *mptr2++ = j + getpalookupsh(mulscale24(krecipasm(m2), globvis));
                m2 += l;
            }
            
            globalx3 = (globalx2 >> 10);
            globaly3 = (globaly2 >> 10);
            asm3 = mulscale16(y2, globalzd) + (globalzx >> 6);
            
            // Set gpinc for vertical drawing (pitch = screen width in bytes)
            gpinc = xdimen;
            
            // Calculate screen address for this column
            intptr_t screen_addr = (intptr_t)render_buffer->pixels + (y2 * xdimen + x) * sizeof(uint32_t);
            
            // Draw the column using slopevlin
            slopevlin(screen_addr, krecipasm(asm3 >> 3), (intptr_t)nptr2, y2 - y1 + 1, globalx1, globaly1);
        }
        
next_most:
        globalx2 += globalx;
        globaly2 += globaly;
        globalzx += globalz;
        shoffs += shinc;
    }
}
