/*
*   This file is part of Anemone3DS
*   Copyright (C) 2016-2017 Alex Taber ("astronautlevel"), Dawid Eckert ("daedreth")
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include "fs.h"
#include "themes.h"
#include "splashes.h"
#include "draw.h"
#include "common.h"
#include "camera.h"
#include <time.h>

int __stacksize__ = 64 * 1024;
Result archive_result;

int init_services(void)
{
    cfguInit();
    ptmuInit();
    httpcInit(0);
    archive_result = open_archives();
    homebrew = true;
    if (!envIsHomebrew())
    {
        homebrew = false;
    } else {
        s64 out;
        svcGetSystemInfo(&out, 0x10000, 0);
        if (out)
        {
            homebrew = false;
        }
    }
    return homebrew;
}

int exit_services(void)
{
    close_archives();
    cfguExit();
    ptmuExit();
    httpcExit();
    return 0;
}

int main(void)
{
    srand(time(NULL));
    bool homebrew = init_services();
    init_screens();
    
    themes_list = NULL;
    theme_count = 0;
    Result res = get_themes(&themes_list, &theme_count);
    if (R_FAILED(res))
    {
        //don't need to worry about possible textures (icons, previews), that's freed by pp2d itself
        free(themes_list);
        themes_list = NULL;
    }
    splash_count = 0;
    splashes_list = NULL;
    res = get_splashes(&splashes_list, &splash_count);
    if (R_FAILED(res))
    {
        //don't need to worry about possible textures (icons, previews), that's freed by pp2d itself
        free(splashes_list);
        splashes_list = NULL;
    }

    splash_mode = false;
    int selected_splash = 0;
    int selected_theme = 0;
    int previously_selected = 0;
    int shuffle_theme_count = 0;
    bool preview_mode = false;
    
    while(aptMainLoop())
    {
        hidScanInput();
        u32 kDown = hidKeysDown();
        u32 kHeld = hidKeysHeld();
        
        if (qr_mode) 
        {
            take_picture();
        } else if (!splash_mode)
        {
            draw_theme_interface(themes_list, theme_count, selected_theme, preview_mode, shuffle_theme_count);
        } else {
            draw_splash_interface(splashes_list, splash_count, selected_splash, preview_mode);
        }
        
        if (kDown & KEY_START)
        {
            if (homebrew)
                APT_HardwareResetAsync();
            else {
                srvPublishToSubscriber(0x202, 0);
            }
        }
        else if (kDown & KEY_L)
        {
            splash_mode = !splash_mode;
        }
        
        if (R_FAILED(archive_result) && !splash_mode)
        {
            draw_themext_error();
            continue;
        }
        
        if (kDown & KEY_R)
        {
            if (preview_mode) {
                continue;
            } else {
                qr_mode = !qr_mode;
                if (qr_mode) init_qr();
                else exit_qr();
                continue;
            }
        }

        if (qr_mode) continue;

        if (themes_list == NULL && !splash_mode)
            continue;
        
        if (splashes_list == NULL && splash_mode)
            continue;

        Theme_s * current_theme = &themes_list[selected_theme];
        Splash_s *current_splash = &splashes_list[selected_splash];
        
        if (kDown & KEY_Y)
        {
            if (!preview_mode)
            {
                if (!splash_mode)
                {
                    if (!current_theme->has_preview)
                        load_theme_preview(current_theme);
                    
                    preview_mode = current_theme->has_preview;
                } else {
                    load_splash_preview(current_splash);
                    preview_mode = true;
                }
            }
            else
                preview_mode = false;
        }

        //don't allow anything while the preview is up
        if (preview_mode)
            continue;
        
        // Actions
        else if (kDown & KEY_X)
        {
            if (splash_mode) {
                draw_splash_install(UNINSTALL);
                splash_delete();
            } else {
                draw_theme_install(BGM_INSTALL);
                bgm_install(*current_theme);
            }
        }
        else if (kDown & KEY_A)
        {
            if (splash_mode)
            {
                draw_splash_install(SINGLE_INSTALL);
                splash_install(*current_splash);
                svcSleepThread(5e8);
            } else {
                draw_theme_install(SINGLE_INSTALL);
                single_install(*current_theme);
            }
            //these two are here just so I don't forget how to implement them - HM
            //del_theme(current_theme->path);
            //get_themes(&themes_list, &theme_count);
        }
        
        else if (kDown & KEY_B)
        {
            if (splash_mode)
            {

            } else {
                if (shuffle_theme_count < 10)
                {
                    if (current_theme->in_shuffle) shuffle_theme_count--;
                    else shuffle_theme_count++;
                    current_theme->in_shuffle = !(current_theme->in_shuffle);
                } else {
                    if (current_theme->in_shuffle) {
                        shuffle_theme_count--;
                        current_theme->in_shuffle = false;
                    } 
                }
            }
        }

        else if (kDown & KEY_SELECT)
        {
            if (splash_mode)
            {

            } else {
                if (shuffle_theme_count > 0)
                {
                    draw_theme_install(SHUFFLE_INSTALL);
                    shuffle_install(themes_list, theme_count);
                    shuffle_theme_count = 0;
                }
            }
        }

        // Movement in the UI
        else if (kDown & KEY_DOWN) 
        {
            if (splash_mode)
            {
                selected_splash++;
                if (selected_splash >= splash_count)
                    selected_splash = 0;
            } else {
                selected_theme++;
                if (selected_theme >= theme_count)
                    selected_theme = 0;
            }
        }
        else if (kDown & KEY_UP)
        {
            if (splash_mode)
            {
                selected_splash--;
                if (selected_splash < 0)
                    selected_splash = splash_count - 1;
            } else {
                selected_theme--;
                if (selected_theme < 0)
                    selected_theme = theme_count - 1;
            }
        }
        // Quick moving
        else if (kDown & KEY_LEFT) 
        {
            if (splash_mode) 
            {
                selected_splash -= 4;
                if (selected_splash < 0) selected_splash = 0;
            } else {
                selected_theme -= 4;
                if (selected_theme < 0) selected_theme = 0;
            }
        }
        else if (kDown & KEY_RIGHT)
        {
            if (splash_mode) 
            {
                selected_splash += 4;
                if (selected_splash >= splash_count) selected_splash = splash_count-1;
            } else {
                selected_theme += 4;
                if (selected_theme >= theme_count) selected_theme = theme_count-1;
            }
        }
        // Fast scroll using circle pad
        else if (kHeld & KEY_CPAD_UP)
        {
            svcSleepThread(100000000);

            if (splash_mode)
            {
                selected_splash--;
                if (selected_splash < 0)
                    selected_splash = splash_count - 1;
            } else {
                selected_theme--;
                if (selected_theme < 0)
                    selected_theme = theme_count - 1;
            }
        }
        else if (kHeld & KEY_CPAD_DOWN)
        {
            svcSleepThread(100000000);
            
            if (splash_mode)
            {
                selected_splash++;
                if (selected_splash >= splash_count)
                    selected_splash = 0;
            } else {
                selected_theme++;
                if (selected_theme >= theme_count)
                    selected_theme = 0;
            }
        }
        else if (kDown & KEY_CPAD_LEFT) 
        {
            svcSleepThread(100000000);

            if (splash_mode) 
            {
                selected_splash -= 4;
                if (selected_splash < 0) selected_splash = 0;
            } else {
                selected_theme -= 4;
                if (selected_theme < 0) selected_theme = 0;
            }
        }
        else if (kDown & KEY_CPAD_RIGHT)
        {
            svcSleepThread(100000000);
            
            if (splash_mode) 
            {
                selected_splash += 4;
                if (selected_splash >= splash_count) selected_splash = splash_count-1;
            } else {
                selected_theme += 4;
                if (selected_theme >= theme_count) selected_theme = theme_count-1;
            }
        }
        
        if (!splash_mode && selected_theme != previously_selected)
        {
            current_theme->has_preview = false;
            previously_selected = selected_theme;
        }
    }
    exit_screens();
    exit_services();

    return 0;
}
