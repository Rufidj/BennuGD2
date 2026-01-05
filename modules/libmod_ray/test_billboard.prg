import "libmod_gfx";
import "libmod_input";
import "libmod_misc";
import "libmod_sound";
import "libmod_ray";

global
    int fpg_textures;
    int fpg_santa;
    float move_speed = 10.0;
    float rot_speed = 0.05;
    int screen_w = 800;
    int screen_h = 600;
    int musica_fondo;
    int sonido_santa[2];  // Array de sonidos de Santa
    int tree_0;
end

process main()
begin
    // Inicializar ventana y motor
    set_mode(800, 600, 32);
    set_fps(60, 0);
    
    // Inicializar motor raycasting
    RAY_INIT(800, 600, 60, 4);
    
    // Carga de Trees 
    tree_0 = map_load("tree.png");
    // Cargar FPG de texturas
    fpg_textures = fpg_load("textures2.fpg");

    
    // Cargar FPG de sprites Santa
    fpg_santa = fpg_load("santa.fpg");
 
    
    // Cargar mapa
    if (RAY_LOAD_MAP("test.raymap", fpg_textures) == 0)
        say("Error: No se pudo cargar el mapa Maps/M1N1.raymap");
        return;
    end
    
    // Activar billboard con 12 direcciones
    RAY_SET_BILLBOARD(1, 12);
    
    // Activar fog (niebla)
    // RAY_SET_FOG(enabled, r, g, b, start_distance, end_distance)
    RAY_SET_FOG(1, 255, 255, 255, 512.0, 2048.0);  // Niebla blanca desde 4 baldosas hasta 16 baldosas
    
    // Configurar minimapa (esquina inferior derecha)
    // RAY_SET_MINIMAP(enabled, size, x, y, scale)
    RAY_SET_MINIMAP(1, 250, 590, 390, 0.2);  // 250x250 pixels, posición (590, 390), escala 0.2
    
    
    // Cargar y reproducir música de fondo
    musica_fondo = music_load("Music/christmas-happy-background.ogg");
    if (musica_fondo > 0)
        music_set_volume(60);  // Volumen al 60% (0-128)
        music_play(musica_fondo, 0);  // 0 = reproducir una vez, -1 = loop infinito
    end
    
    // Cargar sonidos de Santa
    sonido_santa[0] = sound_load("Sounds/santa-claus-EN.wav");
    sonido_santa[1] = sound_load("Sounds/santa-claus-ES.wav");
    
    // Crear los 6 procesos Santa (flags 1-6)
    santa(1);
    santa(2);
    santa(3);
    santa(4);
    santa(5);
    santa(6);
    tree(7);
    tree(8);
    tree(9);
    tree(10);
    tree(11);
    tree(12);
    tree(13);
    tree(14);
    tree(15);

    
    say("Controles:");
    say("W/S - Adelante/Atrás");
    say("A/D - Izquierda/Derecha (strafe)");
    say("Flechas - Rotar cámara");
    say("E - Abrir/Cerrar puertas");
    say("ESC - Salir");
    
    loop
        // Controles de movimiento FPS
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
        
        // Rotación con flechas
        if (key(_left))
            RAY_ROTATE(-rot_speed);
        end
        
        if (key(_right))
            RAY_ROTATE(rot_speed);
        end
        
        // Abrir/cerrar puertas con E
        if (key(_e))
            RAY_TOGGLE_DOOR();
            while (key(_e)) frame; end  // Esperar a que suelte la tecla
        end
        
        // Salir con ESC
        if (key(_esc))
            exit();
        end
        
        // Renderizar frame
       graph = RAY_RENDER();
        if (graph)
            x = screen_w / 2;
            y = screen_h / 2;
            size = 100;
        end
        frame;
    end
end

process santa(int flag_spawn)
private
    float pos_x, pos_y, pos_z;
    int andar_frente = 4;
    int max_andar_frente = 6;
    int andar_atras = 10;
    int max_andar_atras = 12;
    int anda_izq = 1;
    int max_anda_izq = 3;
    int anda_der = 7;
    int max_anda_der = 9;
    
    // Variables de movimiento
    float velocidad = 2.0;
    float vel_x, vel_y;
    float new_x, new_y;
    int direccion = 0;  // 0=frente, 1=atras, 2=izq, 3=der
    int tiempo_cambio = 0;
    int tiempo_max_cambio = 60;  // Cambiar dirección cada 60 frames (~1 segundo)
    
    // Variables de animación
    int frame_anim = 0;
    int contador_anim = 0;
    int velocidad_anim = 5;  // Cambiar frame cada 5 frames
    
    // Variables de proximidad al jugador
    float distancia_jugador;
    float cam_x, cam_y;
    int sonido_reproducido = 0;  // Flag para evitar reproducir múltiples veces
    float distancia_minima = 640.0;  // 5 baldosas * 128 unidades/baldosa
    
begin
    // Establecer la flag de spawn para este proceso
    if (RAY_SET_FLAG(flag_spawn) == 0)
        say("Error: No se pudo establecer flag " + flag_spawn);
        return;
    end
    
    // Obtener posición de la flag
    pos_x = RAY_GET_FLAG_X(flag_spawn);
    pos_y = RAY_GET_FLAG_Y(flag_spawn);
    pos_z = RAY_GET_FLAG_Z(flag_spawn);
    
    say("Santa " + flag_spawn + " spawneado en (" + pos_x + ", " + pos_y + ", " + pos_z + ")");
    
    // El gráfico del proceso será usado por el billboard
    // Asegúrate de que santa.fpg tiene los frames 0-11 del sprite Santa
    file = fpg_santa;
    graph = andar_frente;
    
    // Elegir dirección inicial aleatoria
    direccion = rand(0, 3);
    tiempo_cambio = rand(30, tiempo_max_cambio);
    
    loop
        // Actualizar temporizador de cambio de dirección
        tiempo_cambio--;
        if (tiempo_cambio <= 0)
            // Cambiar a dirección aleatoria
            direccion = rand(0, 3);
            tiempo_cambio = rand(30, tiempo_max_cambio);
        end
        
        // Calcular velocidad según dirección
        vel_x = 0;
        vel_y = 0;
        
        switch (direccion)
            case 0:  // Frente (hacia abajo en Y)
                vel_y = velocidad;
                frame_anim = andar_frente + (contador_anim / velocidad_anim) % (max_andar_frente - andar_frente + 1);
            end
            
            case 1:  // Atrás (hacia arriba en Y)
                vel_y = -velocidad;
                frame_anim = andar_atras + (contador_anim / velocidad_anim) % (max_andar_atras - andar_atras + 1);
            end
            
            case 2:  // Izquierda
                vel_x = -velocidad;
                frame_anim = anda_izq + (contador_anim / velocidad_anim) % (max_anda_izq - anda_izq + 1);
            end
            
            case 3:  // Derecha
                vel_x = velocidad;
                frame_anim = anda_der + (contador_anim / velocidad_anim) % (max_anda_der - anda_der + 1);
            end
        end
        
        // Calcular nueva posición
        new_x = pos_x + vel_x;
        new_y = pos_y + vel_y;
        
        // Verificar colisión ANTES de moverse
        if (RAY_CHECK_COLLISION(new_x, new_y, 20.0) == 0)
            // No hay colisión, actualizar posición
            pos_x = new_x;
            pos_y = new_y;
            
            // Actualizar posición del sprite en el motor
            RAY_UPDATE_SPRITE_POSITION(pos_x, pos_y, pos_z);
            
            // Actualizar animación
            contador_anim++;
            graph = frame_anim;
        else
            // Hay colisión, cambiar dirección inmediatamente
            direccion = rand(0, 3);
            tiempo_cambio = rand(30, tiempo_max_cambio);
        end
        
        // Detectar proximidad con el jugado say("Árbol " + flag_spawn + " spawneado en (" + pos_x + ", " + pos_y + ", " + pos_z + ")");r
        cam_x = RAY_GET_CAMERA_X();
        cam_y = RAY_GET_CAMERA_Y();
        
        // Calcular distancia al jugador
        distancia_jugador = sqrt((pos_x - cam_x) * (pos_x - cam_x) + (pos_y - cam_y) * (pos_y - cam_y));
        
        // Si está cerca y no ha reproducido sonido
        if (distancia_jugador < distancia_minima && sonido_reproducido == 0)
            // Reproducir sonido aleatorio
            sound_play(sonido_santa[rand(0, 1)], 0);
            sonido_reproducido = 1;  // Marcar como reproducido
        end
        
        // Si se aleja, resetear flag para poder reproducir de nuevo
        if (distancia_jugador >= distancia_minima * 1.5)
            sonido_reproducido = 0;
        end
        
        frame;
    end
end

process tree(flag_spawn)
begin
    // Establecer la flag de spawn para este proceso
    if (RAY_SET_FLAG(flag_spawn) == 0)
        say("Error: No se pudo establecer flag " + flag_spawn);
        return;
    end

    // El gráfico del proceso será usado por el billboard
    
    graph = tree_0;

    loop
        frame;
    end
end