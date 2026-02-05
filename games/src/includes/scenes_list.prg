// Auto-generated list of scenes
declare process menu_2();
declare process menu();

include "scenes/menu_2.prg"
include "scenes/menu.prg"

// Scene Navigation Helper
function goto_scene(string name)
begin
    let_me_alone();
    write_delete(all_text);
    
    if(name == "menu_2") menu_2();
    else if(name == "menu") menu();
    else say("Scene not found: " + name);
end

