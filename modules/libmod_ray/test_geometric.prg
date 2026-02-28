// test_geometric.prg
// Programa de prueba para el motor geométrico (sectores Build Engine-style)
import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_ray";

GLOBAL
    int screen_w = 800;
    int screen_h = 600;
    int fpg_textures;
    int fog_enabled = 0;
    int minimap_enabled = 1;
    int pitch;
    int skin_id;
END

PROCESS main()
PRIVATE
    float move_speed = 5.0;
    float rot_speed = 0.05;
    float pitch_speed = 0.02;
    int fog_key_pressed = 0;
    int minimap_key_pressed = 0;
    float cam_x, cam_y, cam_z;
    int cam_sector;
BEGIN
    set_mode(screen_w, screen_h);
    set_fps(60, 0);
    window_set_title("Geometric Sector Engine Test");

    // Cargar FPG con texturas
    fpg_textures = fpg_load("textures2.fpg");
    if (fpg_textures < 0)
        say("ERROR: No se pudo cargar textures.fpg");
                 skin_id = 1;
        say("Crea un FPG con texturas o usa el editor para cargar uno");
        exit();
    end
    
    // Inicializar motor de raycasting
    // Parámetros: width, height, FOV, strip_width
    // FOV aumentado a 110° para mejor visibilidad en habitaciones pequeñas
    if (RAY_INIT(screen_w, screen_h, 110, 1) == 0)
        say("ERROR: No se pudo inicializar el motor");
        exit();
    end
    
    // Cargar mapa .raymap (formato v8 - sectores geométricos)
    if (RAY_LOAD_MAP("test.raymap", fpg_textures) == 0)
        say("ERROR: No se pudo cargar el mapa test_v8.raymap");
        say("Ejecuta primero: bgdi create_test_map.prg");
        RAY_SHUTDOWN();
        exit();
    end
    
    // Configurar posición inicial de la cámara
    // La posición se carga automáticamente del mapa, pero forzamos altura para ver suelo
    // Si el mapa no tiene cámara, se usa (384, 384, 0) por defecto
    // Forzamos Z=32 para que no esté a ras de suelo y se vea el suelo
   // RAY_SET_CAMERA(384.0, 384.0, 32.0, 0.0, 0.0);
    
    // Configurar opciones de renderizado
    RAY_SET_FOG(fog_enabled, 150, 150, 180, 512.0, 1280.0);
    RAY_SET_DRAW_MINIMAP(minimap_enabled);
    RAY_SET_DRAW_WEAPON(0);
    RAY_SET_BILLBOARD(1, 12);
    
    // ===== CARGA DE MODELO MD2 DE PRUEBA =====
    int md2_model = RAY_LOAD_MD2("Models/tris.md2");
    float md2_angle = 0.0;
    int sprite_id = -1;
    if (md2_model)
        // CARGAR TEXTURA PCX
        int skin_id = map_load("Models/base.pcx");
        if (skin_id == 0) 
             say("Error cargando Models/tris.pcx. Usando textura 1 del FPG.");
             skin_id = 1;
        end

        // Intentar obtener Flag 2
        float sx = RAY_GET_FLAG_X(2);
        float sy = RAY_GET_FLAG_Y(2);
        float sz = RAY_GET_FLAG_Z(2);
        
        if (sx == 0 && sy == 0)
            say("Flag 2 no encontrado. Intentando Flag 1...");
            sx = RAY_GET_FLAG_X(1);
            sy = RAY_GET_FLAG_Y(1);
            sz = RAY_GET_FLAG_Z(1);
        end
        

        
        if (sx != 0 || sy != 0) // Si se encontró algun flag
            sz = RAY_GET_FLOOR_HEIGHT(sx, sy);
            say("Creando sprite en spawn: " + sx + ", " + sy + ", " + sz);
            sprite_id = RAY_ADD_SPRITE(sx, sy, sz, 0, 1, 96, 96, 0); 
        else
            // Fallback: Delante de la cámara (384, 384)
            // Ponemos en 500, 384
            float fz = RAY_GET_FLOOR_HEIGHT(500.0, 384.0);
            say("Ningun flag encontrado. Usando coordenadas fallback (delante camara) Z=" + fz);
            sprite_id = RAY_ADD_SPRITE(500.0, 384.0, fz, 0, 1, 96, 96, 0);
        end
        
        RAY_SET_SPRITE_MD2(sprite_id, md2_model, skin_id);
    else
        say("Error cargando Models/tris.md2");
    end
    
    // Iniciar proceso de renderizado
    ray_display();
    
    LOOP
        // ===== CONTROLES DE MOVIMIENTO =====
        if (key(_w)) 
            RAY_MOVE_FORWARD(move_speed); 
        end
        
        if (key(_s)) 
            RAY_MOVE_BACKWARD(move_speed); 
        end
        
        if (key(_a)) 
            RAY_STRAFE_LEFT(move_speed); 
        end
        
        if (key(_d)) 
            RAY_STRAFE_RIGHT(move_speed); 
        end
        
        // ===== CONTROLES DE CÁMARA =====
        if (key(_left)) 
            RAY_ROTATE(-rot_speed); 
        end
        
        if (key(_right)) 
            RAY_ROTATE(rot_speed); 
        end
        
        if (key(_up)) 
            RAY_LOOK_UP_DOWN(pitch_speed); 
        end
        
        if (key(_down)) 
            RAY_LOOK_UP_DOWN(-pitch_speed); 
        end

        // ===== ALTURA DE CÁMARA (DEBUG) =====
        if (key(_q))
            cam_x = RAY_GET_CAMERA_X();
            cam_y = RAY_GET_CAMERA_Y();
            cam_z = RAY_GET_CAMERA_Z();
            float tmp_angle = RAY_GET_CAMERA_ROT();
            pitch = RAY_GET_CAMERA_PITCH();
            RAY_SET_CAMERA(cam_x, cam_y, cam_z + 2.0, tmp_angle, pitch);
        end
        
        if (key(_e))
             cam_x = RAY_GET_CAMERA_X();
            cam_y = RAY_GET_CAMERA_Y();
            cam_z = RAY_GET_CAMERA_Z();
            float tmp_angle = RAY_GET_CAMERA_ROT();
            pitch = RAY_GET_CAMERA_PITCH();
            RAY_SET_CAMERA(cam_x, cam_y, cam_z - 2.0, tmp_angle, pitch);
        end
        
        // Rotar modelo MD2
        if (sprite_id != -1)
            md2_angle += 2.0;
            if (md2_angle > 360.0) md2_angle -= 360.0; end
            RAY_SET_SPRITE_ANGLE(sprite_id, md2_angle);
        end
        
        // ===== SALTO =====
        if (key(_space)) 
            RAY_JUMP(); 
        end
        
        // ===== OPCIONES DE VISUALIZACIÓN =====
        if (key(_f))
            if (fog_key_pressed == 0)
                fog_enabled = !fog_enabled;
                RAY_SET_FOG(fog_enabled, 150, 150, 180, 512.0, 1280.0);
                fog_key_pressed = 1;
            end
        else
            fog_key_pressed = 0;
        end
        
        if (key(_m))
            if (minimap_key_pressed == 0)
                minimap_enabled = !minimap_enabled;
                RAY_SET_DRAW_MINIMAP(minimap_enabled);
                minimap_key_pressed = 1;
            end
        else
            minimap_key_pressed = 0;
        end

        if (key(_esc));
            LET_ME_ALONE();
            exit("Gracias por probar el motor geométrico!");
            end
        
        // ===== INFORMACIÓN EN PANTALLA =====
        // write(0, 10, 10, 0, "WASD: Mover | Flechas: Rotar camara | Espacio: Saltar");
        // write(0, 10, 30, 0, "F: Niebla | M: Minimapa | ESC: Salir");
        
        // Mostrar posición (útil para debug)
        cam_x = RAY_GET_CAMERA_X();
        cam_y = RAY_GET_CAMERA_Y();
        cam_z = RAY_GET_CAMERA_Z();
        // Asumiendo que RAY_GET_CAMERA_SECTOR retorna int
        cam_sector = RAY_GET_CAMERA_SECTOR();
        // write(0, 10, 50, 0, "Pos: (" + cam_x + ", " + cam_y + ", " + cam_z + ")");
        // write(0, 10, 70, 0, "Fog: " + fog_enabled + " | Minimap: " + minimap_enabled);
         write(0, 10, 90, 0, "FPS: " + frame_info.fps + " | Sector: " + cam_sector);
        
        if (key(_esc)) 
            break; 
        end
        
        FRAME;
        WRITE_DELETE(all_text);
    END
    
    // Limpiar recursos
    RAY_FREE_MAP();
    RAY_SHUTDOWN();

    fpg_unload(fpg_textures);
END

PROCESS ray_display()
BEGIN
    LOOP
        // Renderizar el frame del raycasting
        graph = RAY_RENDER(0);
        x = screen_w / 2;
        y = screen_h / 2;
        size = 100;
        
        FRAME;
    END
END
