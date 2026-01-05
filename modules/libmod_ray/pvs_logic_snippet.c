/* ============================================================================
   STATIC PVS (Potentially Visible Set) BAKING
   ============================================================================ */

static void ray_bake_pvs_recursive(int source_id, int current_id, int depth, uint8_t *visited) {
    if (depth <= 0) return;
    
    // Mark as visible
    // Matrix is flat: [source * num_sectors + current]
    int idx = source_id * g_engine.num_sectors + current_id;
    g_engine.pvs_matrix[idx] = 1;
    
    RAY_Sector *sector = &g_engine.sectors[current_id];
    
    // Traverse portals
    for (int p = 0; p < sector->num_portals; p++) {
        int portal_id = sector->portal_ids[p];
        if (portal_id < 0 || portal_id >= g_engine.num_portals) continue;
        
        RAY_Portal *portal = &g_engine.portals[portal_id];
        
        int next_id = -1;
        if (portal->sector_a == current_id) next_id = portal->sector_b;
        else if (portal->sector_b == current_id) next_id = portal->sector_a;
        
        if (next_id != -1 && !visited[next_id]) {
            visited[next_id] = 1;
            // Recurse with decremented depth
            // Depth 32 covers most reasonable maps without overflowing
            ray_bake_pvs_recursive(source_id, next_id, depth - 1, visited);
            visited[next_id] = 0; // Backtrack (actually... PVS is union of all paths, so we don't strictly need to unvisit for *other* paths, but for standard limited BFS, visited is per traversal from source. Wait, standard BFS doesn't unvisit. We want "Are you reachable?". So `visited` prevents loops. Correct.)
        }
    }
}

void ray_bake_pvs() {
    if (g_engine.num_sectors == 0) return;
    
    printf("RAY: Baking Static PVS for %d sectors...\n", g_engine.num_sectors);
    
    // Allocate Matrix (num * num bytes)
    if (g_engine.pvs_matrix) free(g_engine.pvs_matrix);
    g_engine.pvs_matrix = (uint8_t*)calloc(g_engine.num_sectors * g_engine.num_sectors, 1);
    
    if (!g_engine.pvs_matrix) {
        fprintf(stderr, "RAY: Error allocating PVS matrix\n");
        return;
    }
    
    // Temporary visited array for recursion
    uint8_t *visited = (uint8_t*)malloc(g_engine.num_sectors);
    
    for (int i = 0; i < g_engine.num_sectors; i++) {
        // Clear visited for this source sector
        memset(visited, 0, g_engine.num_sectors);
        
        // Setup recursion
        visited[i] = 1;
        
        // Mark self as visible
        g_engine.pvs_matrix[i * g_engine.num_sectors + i] = 1;
        
        // Start traversal (Depth 32 is "infinite" for most Doom-like maps)
        ray_bake_pvs_recursive(i, i, 32, visited);
    }
    
    free(visited);
    g_engine.pvs_ready = 1;
    printf("RAY: PVS Bake Complete.\n");
}
