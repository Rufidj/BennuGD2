import "mod_gfx";
import "mod_input";
import "mod_misc";
import "mod_net";

#define PORT 12346

global
    int px = 320;
    int py = 240;
    int isz;
end

process main()
private
    int *srv;
    int *cli = NULL;
    int *socks;
    int *events;
    int *evt;
    int *ctx;
    int n, i, pairs;
    int data[127];
    int host_ps5;
    int tick;
begin
    set_mode(640, 480);
    isz = sizeof(int);
    host_ps5 = (os_id == 1006);
    ball(host_ps5);

    if (host_ps5)
        say("NETGFX: servidor (PS5) en puerto " + PORT);
        srv = net_open(NET_MODE_SERVER, NET_PROTO_TCP, "", PORT);
        if (srv == 0) say("NETGFX: servidor FALLO"); end
        socks = list_create();
        events = list_create();
        list_insertItem(socks, srv);
        while (!key(_esc))
            n = net_wait(socks, 0, events);
            if (n > 0)
                ctx = NULL;
                while ((evt = list_walk(events, &ctx)))
                    if (net_is_new_connection(evt))
                        say("NETGFX: cliente conectado");
                        list_insertItem(socks, evt);
                    else
                        n = net_recv(evt, &data, isz * 126);
                        if (n <= NET_DISCONNECTED)
                            say("NETGFX: cliente desconectado");
                            net_close(evt);
                            list_removeItem(socks, evt);
                        else
                            pairs = n / (isz * 2);
                            if (pairs > 0)
                                px = data[(pairs - 1) * 2];
                                py = data[(pairs - 1) * 2 + 1];
                            end
                        end
                    end
                end
            end
            frame;
        end
        net_close(srv);
    else
        say("NETGFX: cliente (PC) -> 192.168.1.132:" + PORT);
        cli = net_open(NET_MODE_CLIENT, NET_PROTO_TCP, "192.168.1.132", PORT);
        if (cli == 0)
            say("NETGFX: no pude conectar");
        else
            say("NETGFX: conectado; flechas para mover, ESC sale");
            while (!key(_esc))
                if (key(_left))  px -= 4; end
                if (key(_right)) px += 4; end
                if (key(_up))    py -= 4; end
                if (key(_down))  py += 4; end
                if (px < 24) px = 24; end
                if (px > 616) px = 616; end
                if (py < 24) py = 24; end
                if (py > 456) py = 456; end
                data[0] = px; data[1] = py;
                net_send(cli, &data, isz * 2);
                frame;
            end
            net_close(cli);
        end
    end
end

process ball(isps5)
begin
    graph = map_new(48, 48, 32);
    if (isps5)
        map_clear(0, graph, rgb(255, 80, 80));
    else
        map_clear(0, graph, rgb(80, 255, 80));
    end
    loop
        x = px; y = py;
        frame;
    end
end
