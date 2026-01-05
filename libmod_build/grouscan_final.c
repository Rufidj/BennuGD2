// ============================================================================
// Phase C: grouscan() - Floor/Ceiling rendering (C version)
// Based on EDuke32 engine.cpp:3950-4171
// ============================================================================

static void grouscan(int32_t dax1, int32_t dax2, int32_t sectnum, char dastat)
{
    BUILD_Sector *sec = &g_map->sectors[sectnum];
    int32_t i, l, x, y, dx, dy, wx, wy, y1, y2, daz;
    int32_t daslope, dasqr;
    
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
    BUILD_Tile *tile = build_get_tile(globalpicnum);
    if (!tile || !tile->data) {
        // Fallback to solid color
        uint32_t color = (dastat == 0) ? 
            SDL_MapRGB(gPixelFormat, 64, 64, 128) :  // Ceiling: dark blue
            SDL_MapRGB(gPixelFormat, 128, 64, 32);   // Floor: brown
        
        for (x = dax1; x <= dax2; x++) {
            if (dastat == 0) {
                y1 = umost[x];
                y2 = (dmost[x] < uplc[x]) ? dmost[x] : uplc[x];
                y2--;
            } else {
                y1 = (umost[x] > dplc[x]) ? umost[x] : dplc[x];
                y2 = dmost[x] - 1;
            }
            
            for (y = y1; y <= y2 && y < ydimen; y++)
                gr_put_pixel(render_buffer, x, y, color);
        }
        return;
    }
    
    int tsizx = tile->xsize;
    int tsizy = tile->ysize;
    
    // Get first wall for slope calculations
    BUILD_Wall *wal = &g_map->walls[sec->wallptr];
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
    int j = 8 - (picsiz[globalpicnum] >> 4);
    if (globalorientation & 8) { i++; j++; }
    
    globalx1 <<= (i + 12);
    globaly1 <<= (j + 12);
    
    if (i >= 0) { globalx2 <<= i; globalx <<= i; }
    else { globalx2 >>= -i; globalx >>= -i; }
    
    if (j >= 0) { globaly2 <<= j; globaly <<= j; }
    else { globaly2 >>= -j; globaly >>= -j; }
    
    // Apply panning
    if (dastat == 0) {
        globalx1 += (uint32_t)sec->ceilingxpanning << 24;
        globaly1 += (uint32_t)sec->ceilingypanning << 24;
    } else {
        globalx1 += (uint32_t)sec->floorxpanning << 24;
        globaly1 += (uint32_t)sec->floorypanning << 24;
    }
    
    // Visibility calculation
    int32_t vis = (sec->visibility != 0) ? 
        mulscale4(globalvisibility, (uint8_t)(sec->visibility + 16)) : globalvisibility;
    globvis = ((((int64_t)(vis * (int64_t)daz)) >> 13) * xdimscale) >> 16;
    
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
            // Calculate perspective-correct texture coordinates for this column
            for (y = y1; y <= y2 && y < ydimen; y++) {
                // Calculate distance to this pixel
                int32_t yoff = y - globalhoriz;
                if (yoff == -1) yoff = -2;  // Avoid division by zero
                int32_t dist = (globalzd / (yoff + 1));
                
                // Calculate world position
                int32_t u = globalx1 + mulscale16(globalx2 + mulscale16(globalx, dist), dist);
                int32_t v = globaly1 + mulscale16(globaly2 + mulscale16(globaly, dist), dist);
                
                // Convert to texture coordinates
                int tx = (u >> 24) & (tsizx - 1);
                int ty = (v >> 24) & (tsizy - 1);
                
                // Get palette index
                int palindex = tile->data[ty * tsizx + tx];
                
                // Apply shading
                int shade = globalshade + (dist >> 16);
                if (shade < 0) shade = 0;
                if (shade > (g_numshades - 1)) shade = g_numshades - 1;
                
                if (g_palookup && shade * 256 < g_numshades * 256)
                    palindex = g_palookup[shade * 256 + palindex];
                
                // Draw pixel
                uint32_t color = g_palette_cache[palindex];
                gr_put_pixel(render_buffer, x, y, color);
            }
        }
        
        // Update perspective vars for next column
        globalx2 += globalx;
        globaly2 += globaly;
        globalzx += globalz;
    }
}

