// Scene: menu_2
// Generated automatically by RayMap Editor. DO NOT EDIT sections outside User Blocks.

process menu_2()
PRIVATE
    int ent_id;
    int id_car_png;
    int id_level1_fpg;
begin
    write_delete(all_text);
    // Load Resources
    id_car_png = map_load("../../BennuGD2/modules/libmod_ray/car.png");
    if(id_car_png <= 0) say("ERROR Loading: ../../BennuGD2/modules/libmod_ray/car.png"); end
    id_level1_fpg = fpg_load("assets/fpg/level1.fpg");
    if(id_level1_fpg <= 0) say("ERROR Loading: assets/fpg/level1.fpg"); end

    // Setup Input (Mouse)
    mouse.file = 0;
    mouse.graph = id_car_png;

    // Instantiate Entities
    // Entity: level1
    ent_id = StaticSprite();
    ent_id.file = id_level1_fpg;
    ent_id.graph = 33;
    ent_id.x = 329; ent_id.y = 238; ent_id.z = 0;
    ent_id.angle = 0; ent_id.size = 100;

    // Text Entity: btn_menu
    Auto_Btn_btn_menu_8792();

    // [[USER_SETUP]]
    // Add your setup code here
    // [[USER_SETUP_END]]
    loop
        // [[USER_LOOP]]
        // Add your LOOP code here
        // [[USER_LOOP_END]]
        frame;
    end
end

// --- Auto-Generated Helper Processes ---

process Auto_Btn_btn_menu_8792()
PRIVATE
    int txt_id;
    int w, h;
begin
    x = 290; y = 243;
    txt_id = write(0, x, y, 0, "MENU 2!!!");
    w = text_width(0, "MENU 2!!!");
    h = text_height(0, "MENU 2!!!");
    loop
        if (mouse.left AND (mouse.x > x AND mouse.x < (x + w)) AND abs(mouse.y - y) < (h/2 + 5))
             begin
                 goto_scene("menu");
                 while(mouse.left) frame; end
             end
        end
        frame;
    end
end
