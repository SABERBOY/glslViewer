#include "console.h"

int  osc_port = 0;

#ifdef SUPPORT_NCURSES
#include <signal.h>

#include <functional>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#include <ncurses.h>
#include "ada/tools/text.h"

bool have_colors = false;

// Status
WINDOW* stt_win                 = nullptr;
Uniforms* uniforms              = nullptr;
float   stt_win_width_pct       = 0.3f; 
bool    stt_visible             = false;


// Buffers
std::stringstream buffer_cout;
std::stringstream buffer_cerr;

// Command windows
WINDOW* cmd_win                 = nullptr;

// Command state
std::string cmd                 = "";
std::string cmd_suggested       = "";
std::vector<std::string> cmd_history;
size_t      cmd_history_offset  = 0;
size_t      cmd_cursor_offset   = 0;
size_t      cmd_tab_counter     = 0;


void refresh_cmd_win() {
    werase(cmd_win);
    box(cmd_win, 0, 0);

    std::string prompt = "";

    if (osc_port > 0) {
        prompt = "osc://localhost:" + ada::toString(osc_port) + " ";
        if (have_colors) wattron(cmd_win, COLOR_PAIR(4));
        mvwprintw(cmd_win, 1, 1, "%s", prompt.c_str() );
        if (have_colors)wattroff(cmd_win, COLOR_PAIR(4));
    }

    if (have_colors) wattron(cmd_win, COLOR_PAIR(4));
    mvwprintw(cmd_win, 1, 1 + prompt.size() + 2, "%s", cmd_suggested.c_str() );
    if (have_colors) wattroff(cmd_win, COLOR_PAIR(4));

    mvwprintw(cmd_win, 1, 1 + prompt.size(), "> %s", cmd.c_str() );

    wmove(cmd_win, 1, 3 + cmd.size() - cmd_cursor_offset);
    wrefresh(cmd_win);
};

void refresh_stt_win() {
    if (!stt_visible || uniforms == nullptr)
        return;

    werase(stt_win);

    size_t x = 1;
    size_t y = 0;

    if (have_colors) wattron(stt_win, COLOR_PAIR(2));
    // Print Native Uniforms (they carry functions) that are present on the shader
    for (UniformFunctionsList::iterator it= uniforms->functions.begin(); it != uniforms->functions.end(); ++it)
        if (it->second.present && it->second.print)
            mvwprintw(stt_win, y++, x, "%23s  %s", it->first.c_str(), it->second.print().c_str() );

    for (TextureList::iterator it = uniforms->textures.begin(); it != uniforms->textures.end(); ++it)
        mvwprintw(stt_win, y++, x, "%23s  %f,%f", (it->first + "Resolution").c_str(), it->second->getWidth(), it->second->getHeight());

    for (StreamsList::iterator it = uniforms->streams.begin(); it != uniforms->streams.end(); ++it) {
        mvwprintw(stt_win, y++, x, "%23s  %f", (it->first+"CurrentFrame").c_str(), it->second->getCurrentFrame() );
        mvwprintw(stt_win, y++, x, "%23s  %f", (it->first+"TotalFrames").c_str(), it->second->getTotalFrames() );
        mvwprintw(stt_win, y++, x, "%23s  %f", (it->first+"Time").c_str(), it->second->getTime() );
        mvwprintw(stt_win, y++, x, "%23s  %f", (it->first+"Duration").c_str(), it->second->getDuration() );
        mvwprintw(stt_win, y++, x, "%23s  %f", (it->first+"Fps").c_str(), it->second->getFps() );
    }
    if (have_colors) wattroff(stt_win, COLOR_PAIR(2));

    y++;

    for (UniformDataList::iterator it= uniforms->data.begin(); it != uniforms->data.end(); ++it) {
        if (it->second.size == 1)
            mvwprintw(stt_win, y++, x, "%23s  %f", it->first.c_str(), it->second.value[0]);
        else if (it->second.size == 2)
            mvwprintw(stt_win, y++, x, "%23s  %f,%f", it->first.c_str(), it->second.value[0], it->second.value[1]);
        else if (it->second.size == 3)
            mvwprintw(stt_win, y++, x, "%23s  %f,%f,%f", it->first.c_str(), it->second.value[0], it->second.value[1], it->second.value[2]);
        else if (it->second.size == 4)
            mvwprintw(stt_win, y++, x, "%23s  %f,%f,%f,%f", it->first.c_str(), it->second.value[0], it->second.value[1], it->second.value[2], it->second.value[3]);
    }

    wrefresh(stt_win);
}

std::string suggest(std::string _cmd, std::string& _suggestion, CommandList& _commands) {

    if (_cmd.size() == 0) {
        _suggestion = "";
        return "";
    }

    _suggestion = "";
    std::stringstream rta; 

    for (size_t i = 0; i < _commands.size(); i++)
        if ( _commands[i].trigger.rfind(_cmd, 0) == 0) {
            if (_suggestion.size() == 0 || 
                _suggestion.size() > _commands[i].trigger.size())
                _suggestion = _commands[i].trigger;

            rta << std::left << std::setw(27) << _commands[i].formula << " " << _commands[i].description << std::endl;
        }

    // if(uniforms != nullptr)
    //     for (UniformDataList::iterator it = _sandbox.uniforms.data.begin(); it != _sandbox.uniforms.data.end(); ++it) {
    //         if (it->first.rfind(_cmd, 0) == 0) {
    //             if (_suggestion.size() == 0 || 
    //                 _suggestion.size() > it->first.size())
    //                 _suggestion = it->first;

    //             rta << it->first;

    //             for (size_t i = 0; it->second.size; i++)
    //                 rta << ",<value>";
                
    //             rta << std::endl;
    //         }
    //     }

    return rta.str();
}
#endif

void console_sigwinch_handler(int signal) {
    #ifdef SUPPORT_NCURSES
    endwin();
    erase();
    refresh();

    if (stt_visible) {
        wresize(cmd_win, 3, COLS * (1.0-stt_win_width_pct) );

        wresize(stt_win, LINES, COLS * stt_win_width_pct );
        mvwin(stt_win, 0, COLS * (1.0-stt_win_width_pct));
    }
    else
        wresize(cmd_win, 3, COLS);
    cmd_tab_counter = 0;

    refresh_stt_win();
    refresh_cmd_win();
    #endif
}

void console_init(int _osc_port) {
    osc_port = _osc_port;

    #ifdef SUPPORT_NCURSES

    initscr();
    raw();
    if (has_colors()) {
        start_color();
        use_default_colors();

        // init_color(COLOR_BLACK, 0, 0, 0);
        init_color(COLOR_CYAN, 700, 700, 700);
        init_color(COLOR_BLUE, 100, 100, 100);
        init_pair(1, COLOR_WHITE, -1);
        init_pair(2, COLOR_CYAN, -1);
        init_pair(3, COLOR_RED, -1);
        init_pair(4, COLOR_BLUE, COLOR_BLACK);

        have_colors = true;
    }
    cbreak();

    // Create windows
    cmd_win = newwin(3, COLS, 0, 0);
    stt_win = newwin(LINES, COLS * stt_win_width_pct, 0, COLS * (1.0 - stt_win_width_pct));

    // Capture Keys
    keypad(stdscr, true);
    scrollok(stdscr, true);
    noecho();

    // Capture all standard console OUT and ERR
    std::streambuf * old_cout = std::cout.rdbuf(buffer_cout.rdbuf());
    std::streambuf * old_cerr = std::cerr.rdbuf(buffer_cerr.rdbuf());

    refresh_stt_win();
    refresh_cmd_win();
    #endif
}

void console_clear() {
    #ifdef SUPPORT_NCURSES
    cmd = "";
    cmd_suggested = "";
    buffer_cout.str("");
    buffer_cerr.str("");

    cmd_cursor_offset = 0;
    cmd_history_offset = 0;
    cmd_tab_counter = 0;
    #endif
}

void console_refresh() {
    #ifdef SUPPORT_NCURSES
    erase();

    if (buffer_cerr.str().size() > 0) {
        if (have_colors) attron(COLOR_PAIR(3));
        mvprintw(4, 0, "%s", buffer_cerr.str().c_str() );
        if (have_colors) attroff(COLOR_PAIR(3));
    }
    else {
        if (have_colors) attron(COLOR_PAIR(2));
        mvprintw(4, 0, "%s", buffer_cout.str().c_str() );
        if (have_colors) attroff(COLOR_PAIR(2));
    } 

    refresh();

    refresh_stt_win();
    refresh_cmd_win();

    #endif
}

bool console_getline(std::string& _cmd, CommandList& _commands, Sandbox& _sandbox) {
    #ifdef SUPPORT_NCURSES

    if (uniforms == nullptr)
        uniforms = &_sandbox.uniforms;

    suggest(cmd, cmd_suggested, _commands);
    console_refresh();

    int ch = getch();
    cmd_suggested = "";
    if (ch == KEY_STAB || ch == '\t') 
        cmd_tab_counter++;
    else
        cmd_tab_counter = 0;

    if ( ch == '\n' || ch == KEY_ENTER || ch == KEY_EOL) {
        buffer_cout.str("");
        buffer_cerr.str("");
        cmd_history.push_back( cmd );
        cmd_cursor_offset = 0;
        cmd_history_offset = 0;

        _cmd = cmd;
        cmd = "";

        return true;
    }
    else if ( ch == KEY_BACKSPACE || ch == KEY_DC || ch == 127 ) {
        if (cmd.size() > cmd_cursor_offset)
            cmd.erase(cmd.end()-cmd_cursor_offset-1, cmd.end()-cmd_cursor_offset);
    }
    else if ( ch == KEY_STAB || ch == '\t') {
        buffer_cout.str("");
        buffer_cerr.str("");
        if (cmd.size() > 0) {
            if (cmd.find(',') == std::string::npos) {
                std::cout << "Suggestions:\n" << std::endl;
                // cmd_suggested = "";
                std::cout << suggest(cmd, cmd_suggested, _commands);

                if (cmd_tab_counter > 1 && cmd_suggested.size() > 0) {
                    cmd = cmd_suggested;
                    cmd_tab_counter = 0;
                }
                else
                    cmd_suggested = cmd_suggested;
            }
            else {
                std::cout << "Use:" << std::endl;

                for (size_t i = 0; i < _commands.size(); i++)
                    if ( ada::beginsWith(cmd, _commands[i].trigger) )
                        std::cout << "      " << std::left << std::setw(16) << _commands[i].formula << "   " << _commands[i].description << std::endl;

                for (UniformDataList::iterator it = _sandbox.uniforms.data.begin(); it != _sandbox.uniforms.data.end(); ++it) {
                    if ( ada::beginsWith(cmd, it->first) ) {
                        std::cout << it->first;

                        for (size_t i = 0; it->second.size; i++)
                            std::cout << ",<value>";
                        
                        std::cout << std::endl;
                    }
                }

                std::cout << "\nNotes:" << std::endl;
                std::cout << "      - <values> between <...> brakets need to be change for and actual value" << std::endl;
                std::cout << "      - when words are separated by | you must choose one of the options, like: A|B|C" << std::endl;
                std::cout << "      * everything betwee [...] is optative" << std::endl;
            }
        }
    }
    else if ( ch == KEY_BREAK || ch == ' ') {
        cmd += ",";
    }
    else if ( ch == KEY_LEFT) {
        if (cmd_cursor_offset < cmd.size())
            cmd_cursor_offset++;
    }
    else if ( ch == KEY_RIGHT)
        cmd_cursor_offset--;
    else if ( ch == KEY_DOWN ) {
        if (cmd_history.size() > 0 && cmd_history_offset > 0) {
            cmd_history_offset--;
            cmd_cursor_offset = 0;

            if (cmd_history_offset == 0)
                cmd = "";
            else
                cmd = cmd_history[ cmd_history.size() - cmd_history_offset ];
        }
    }
    else if ( ch == KEY_UP ) {
        if (cmd_history_offset < cmd_history.size() - 1)
            cmd_history_offset++;
        cmd_cursor_offset = 0;
        if (cmd_history_offset < cmd_history.size() )
            cmd = cmd_history[ cmd_history.size() - 1 - cmd_history_offset ];
    }
    
    // else if ( ch == KEY_END || ch == KEY_EXIT || ch == 27 || ch == EOF) {
    //     keepRunnig = false;
    //     keepRunnig.store(false);
    //     break;
    // }
    else
        cmd.insert(cmd.end() - cmd_cursor_offset, 1, (char)ch );  
      

    // suggest(cmd, cmd_suggested, _commands);
    #endif

    return false;
}

void console_draw_pct(float _pct) {
    #ifdef SUPPORT_NCURSES
    size_t lines, cols;
    getmaxyx(cmd_win, lines, cols);

    werase(cmd_win);
    box(cmd_win,0, 0);

    size_t l = (cols-4) * _pct;
    wattron(cmd_win, COLOR_PAIR(3));
    for (size_t i = 0; i < cols-4; i++)
        mvwprintw(cmd_win, 1, 2 + i, "%s", (i < l )? "#" : ".");
    wattroff(cmd_win, COLOR_PAIR(3));

    wrefresh(cmd_win);
    #else

    // Delete previous line
    const std::string deleteLine = "\e[2K\r\e[1A";
    std::cout << deleteLine;

    int pct = 100 * _pct;
    std::cout << "// [ ";
    for (int i = 0; i < 50; i++) {
        if (i < pct/2) {
            std::cout << "#";
        }
        else {
            std::cout << ".";
        }
    }
    std::cout << " ] " << pct << "%" << std::endl;

    #endif
}

void console_uniforms( bool _show ) {
    #ifdef SUPPORT_NCURSES
    stt_visible = _show;
    console_sigwinch_handler(0);
    #endif
}

void console_uniforms_refresh() {
    #ifdef SUPPORT_NCURSES
    if (stt_visible) {
        refresh_stt_win();
        wmove(cmd_win, 1, 3 + cmd.size() - cmd_cursor_offset);
        wrefresh(cmd_win);
    }
    #endif
}


void console_end() {
    #ifdef SUPPORT_NCURSES
    endwin();
    #endif
}