#ifdef __EMSCRIPTEN__
#define GLFW_INCLUDE_ES3
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#include <sys/stat.h>


#ifndef PLATFORM_WINDOWS
#include <unistd.h>
#endif

#include <map>
#include <thread>
#include <atomic>
#include <iostream>
#include <fstream>

#include "ada/window.h"
#include "ada/gl/gl.h"
#include "ada/tools/fs.h"
#include "ada/tools/time.h"
#include "ada/tools/text.h"
#include "ada/shaders/defaultShaders.h"

#include "sandbox.h"
#include "types/files.h"

std::string                 version = "2.0.2";
std::string                 name    = "GlslViewer";
std::string                 header  = name + " " + version + " by Patricio Gonzalez Vivo ( patriciogonzalezvivo.com )"; 

// Here is where all the magic happens
Sandbox                     sandbox;

//  List of FILES to watch and the variable to communicate that between process
int                         fileChanged;

// Commands variables
std::vector<std::string>    commandsArgs;    // Execute commands
bool                        commandsExit = false;

std::atomic<bool>           keepRunnig(true);
bool                        screensaver = false;
bool                        bTerminate = false;
bool                        fullFps = false;

void                        commandsInit();

#if !defined(__EMSCRIPTEN__)
void                        printUsage(char * executableName);
void                        fileWatcherThread();
void                        cinWatcherThread();
void                        onExit();

#else

extern "C"  {
    
void command(char* c) {
    commandsArgs.push_back( std::string(c) );
}

void setFrag(char* c) {
    sandbox.setSource(FRAGMENT, std::string(c) );
    sandbox.reloadShaders();
}

void setVert(char* c) {
    sandbox.setSource(VERTEX, std::string(c) );
    sandbox.reloadShaders();
}

char* getFrag() {
    return (char*)sandbox.getSource(FRAGMENT).c_str();
}

char* getVert() {
    return (char*)sandbox.getSource(VERTEX).c_str();
}

}

#endif

// Open Sound Control
#if defined(SUPPORT_OSC)
#include <lo/lo_cpp.h>
std::mutex                  oscMutex;
int                         oscPort = 0;
#endif

#if defined(__EMSCRIPTEN__)
EM_BOOL loop (double time, void* userData) {
#else
void loop() {
#endif
    ada::updateGL();

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    #ifndef __EMSCRIPTEN__
    if (!bTerminate && !fullFps && !sandbox.haveChange()) {
    // If nothing in the scene change skip the frame and try to keep it at 60fps
        std::this_thread::sleep_for(std::chrono::milliseconds( ada::getRestMs() ));
        return;
    }
    #else

    if (sandbox.isReady() && commandsArgs.size() > 0) {
        for (size_t i = 0; i < commandsArgs.size(); i++) {
            commandsRun(commandsArgs[i]);
        }
        commandsArgs.clear();
    }

    #endif

    // Draw Scene
    sandbox.render();

    // Draw Cursor and 2D Debug elements
    sandbox.renderUI();

    // Finish drawing
    sandbox.renderDone();

#ifndef __EMSCRIPTEN__
    if ( bTerminate && sandbox.screenshotFile == "" )
        keepRunnig.store(false);
    else
#endif
        ada::renderGL();

    #if defined(__EMSCRIPTEN__)
    return true;
    #endif
}

// Main program
//============================================================================
int main(int argc, char **argv) {
    // Set the size
    glm::ivec4 window_viewport = glm::ivec4(0);
    window_viewport.z = 512;
    window_viewport.w = 512;

    #if defined(DRIVER_BROADCOM) || defined(DRIVER_GBM) 
    glm::ivec2 screen = ada::getScreenSize();
    window_viewport.z = screen.x;
    window_viewport.w = screen.y;
    #endif

    ada::WindowProperties window_properties;

    bool displayHelp = false;
    bool willLoadGeometry = false;
    bool willLoadTextures = false;
    for (int i = 1; i < argc ; i++) {
        std::string argument = std::string(argv[i]);

        if (        std::string(argv[i]) == "-x" ) {
            if(++i < argc)
                window_viewport.x = ada::toInt(std::string(argv[i]));
            else
                std::cout << "Argument '" << argument << "' should be followed by a <pixels>. Skipping argument." << std::endl;
        }
        else if (   std::string(argv[i]) == "-y" ) {
            if(++i < argc)
                window_viewport.y = ada::toInt(std::string(argv[i]));
            else
                std::cout << "Argument '" << argument << "' should be followed by a <pixels>. Skipping argument." << std::endl;
        }
        else if (   std::string(argv[i]) == "-w" ||
                    std::string(argv[i]) == "--width" ) {
            if(++i < argc)
                window_viewport.z = ada::toInt(std::string(argv[i]));
            else
                std::cout << "Argument '" << argument << "' should be followed by a <pixels>. Skipping argument." << std::endl;
        }
        else if (   std::string(argv[i]) == "-h" ||
                    std::string(argv[i]) == "--height" ) {
            if(++i < argc)
                window_viewport.w = ada::toInt(std::string(argv[i]));
            else
                std::cout << "Argument '" << argument << "' should be followed by a <pixels>. Skipping argument." << std::endl;
        }
        else if (   std::string(argv[i]) == "--fps" ) {
            if(++i < argc)
                ada::setFps( ada::toInt(std::string(argv[i])) );
            else
                std::cout << "Argument '" << argument << "' should be followed by a <pixels>. Skipping argument." << std::endl;
        }
        else if (   std::string(argv[i]) == "--help" ) {
            displayHelp = true;
        }
        #if defined(DRIVER_GBM) 
        else if (   std::string(argv[i]) == "--display") {
            if (++i < argc)
                window_properties.display = std::string(argv[i]);
            else
                std::cout << "Argument '" << argument << "' should be followed by a the display address. Skipping argument." << std::endl;
        }
        #endif
        #if !defined(DRIVER_GLFW)
        else if (   std::string(argv[i]) == "--mouse") {
            if (++i < argc)
                window_properties.mouse = std::string(argv[i]);
            else
                std::cout << "Argument '" << argument << "' should be followed by a the mouse address. Skipping argument." << std::endl;
        }
        #endif
        else if (   std::string(argv[i]) == "--headless" ) {
            window_properties.style = ada::HEADLESS;
        }
        else if (   std::string(argv[i]) == "-f" ||
                    std::string(argv[i]) == "--fullscreen" ) {
            window_properties.style = ada::FULLSCREEN;
        }
        else if (   std::string(argv[i]) == "--holoplay") {
            window_properties.style = ada::HOLOPLAY;
        }
        else if (   std::string(argv[i]) == "-l" ||
                    std::string(argv[i]) == "--life-coding" ){
            #if defined(DRIVER_BROADCOM) || defined(DRIVER_GBM) 
                window_viewport.x = window_viewport.z - 512;
                window_viewport.z = window_viewport.w = 512;
            #else
                window_properties.style = ada::ALLWAYS_ON_TOP;
            #endif
        }
        else if (   std::string(argv[i]) == "-ss" ||
                    std::string(argv[i]) == "--screensaver") {
            window_properties.style = ada::FULLSCREEN;
            screensaver = true;
        }
        else if (   std::string(argv[i]) == "--msaa") {
            window_properties.msaa = 4;
        }
        else if (   std::string(argv[i]) == "--major") {
            if (++i < argc)
                window_properties.major = ada::toInt(std::string(argv[i]));
            else
                std::cout << "Argument '" << argument << "' should be followed by a the OPENGL MAJOR version. Skipping argument." << std::endl;
        }
        else if (   std::string(argv[i]) == "--minor") {
            if (++i < argc)
                window_properties.minor = ada::toInt(std::string(argv[i]));
            else
                std::cout << "Argument '" << argument << "' should be followed by a the OPENGL MINOR version. Skipping argument." << std::endl;
        }
        else if ( ( ada::haveExt(argument,"ply") || ada::haveExt(argument,"PLY") ||
                    ada::haveExt(argument,"obj") || ada::haveExt(argument,"OBJ") ||
                    ada::haveExt(argument,"stl") || ada::haveExt(argument,"STL") ||
                    ada::haveExt(argument,"glb") || ada::haveExt(argument,"GLB") ||
                    ada::haveExt(argument,"gltf") || ada::haveExt(argument,"GLTF") ) ) {
            willLoadGeometry = true;
        }
        else if (   ada::haveExt(argument,"hdr") || ada::haveExt(argument,"HDR") ||
                    ada::haveExt(argument,"png") || ada::haveExt(argument,"PNG") ||
                    ada::haveExt(argument,"tga") || ada::haveExt(argument,"TGA") ||
                    ada::haveExt(argument,"psd") || ada::haveExt(argument,"PSD") ||
                    ada::haveExt(argument,"gif") || ada::haveExt(argument,"GIF") ||
                    ada::haveExt(argument,"bmp") || ada::haveExt(argument,"BMP") ||
                    ada::haveExt(argument,"jpg") || ada::haveExt(argument,"JPG") ||
                    ada::haveExt(argument,"jpeg") || ada::haveExt(argument,"JPEG") ||
                    ada::haveExt(argument,"mov") || ada::haveExt(argument,"MOV") ||
                    ada::haveExt(argument,"mp4") || ada::haveExt(argument,"MP4") ||
                    ada::haveExt(argument,"mpeg") || ada::haveExt(argument,"MPEG") ||
                    argument.rfind("/dev/", 0) == 0 ||
                    argument.rfind("http://", 0) == 0 ||
                    argument.rfind("https://", 0) == 0 ||
                    argument.rfind("rtsp://", 0) == 0 ||
                    argument.rfind("rtmp://", 0) == 0 ) {
            willLoadTextures = true;
        }   
    }

    #ifndef __EMSCRIPTEN__
    if (displayHelp) {
        printUsage( argv[0] );
        exit(0);
    }
    #endif

    // Declare commands
    commandsInit();

    // Initialize openGL context
    ada::initGL(window_viewport, window_properties);
    #ifndef __EMSCRIPTEN__
    ada::setWindowTitle("GlslViewer");
    #endif

    struct stat st;
    //Load the the resources (textures)
    for (int i = 1; i < argc ; i++){
        std::string argument = std::string(argv[i]);

        if (    argument == "-x" || argument == "-y" ||
                argument == "-w" || argument == "--width" ||
                argument == "-h" || argument == "--height" ||
                argument == "--mouse" || argument == "--display" ||
                argument == "--major" || argument == "--minor" ||
                argument == "--fps" ) {
            i++;
        }
        else if (   argument == "-l" || argument == "--headless" ||
                    argument == "--msaa" ) {
        }
        else if (   std::string(argv[i]) == "-f" ||
                    std::string(argv[i]) == "--fullscreen" ) {
        }
        else if (   argument == "--verbose" ) {
            sandbox.verbose = true;
        }
        else if ( argument == "--nocursor" ) {
            sandbox.cursor = false;
        }
        else if ( argument == "--fxaa" ) {
            sandbox.fxaa = true;
        }
        #if defined(SUPPORT_OSC)
        else if ( argument== "-p" || argument == "--port" ) {
            if(++i < argc)
                oscPort = ada::toInt(std::string(argv[i]));
            else
                std::cout << "Argument '" << argument << "' should be followed by an <osc_port>. Skipping argument." << std::endl;
        }
        #endif
        else if ( argument == "-e" ) {
            if(++i < argc)         
                commandsArgs.push_back(std::string(argv[i]));
            else
                std::cout << "Argument '" << argument << "' should be followed by a <command>. Skipping argument." << std::endl;
        }
        else if ( argument == "-E" ) {
            if(++i < argc) {
                commandsArgs.push_back(std::string(argv[i]));
                commandsExit = true;
            }
            else
                std::cout << "Argument '" << argument << "' should be followed by a <command>. Skipping argument." << std::endl;
        }
        else if (argument == "--fullFps" ) {
            fullFps = true;
        }
        else if (   argument == "-vFlip" ||
                    argument == "--vFlip" ) {
            sandbox.vFlip = false;
        }
        else if ( argument == "--video" ) {
            if (++i < argc) {
                argument = std::string(argv[i]);
                if ( sandbox.uniforms.addStreamingTexture("u_tex" + ada::toString(sandbox.textureCounter), argument,  sandbox.vFlip, true) )
                    sandbox.textureCounter++;
            }
        }
        else if ( argument == "--audio" || argument == "-a" ) {
            std::string device_id = "-1"; //default device id
            // device_id is optional argument, not iterate yet
            if ((i + 1) < argc) {
                argument = std::string(argv[i + 1]);
                if (ada::isInt(argument)) {
                    device_id = argument;
                    i++;
                }
            }
            if (sandbox.uniforms.addAudioTexture("u_tex" + ada::toString(sandbox.textureCounter), device_id,  sandbox.vFlip, true) )
                sandbox.textureCounter++;
        }
        else if ( argument == "--holoplay" ) {
            if (++i < argc) {
                sandbox.holoplay = ada::toInt(argv[i]);
            }
        }
        else if ( argument == "-c" || argument == "-sh" ) {
            if(++i < argc) {
                argument = std::string(argv[i]);
                sandbox.uniforms.setCubeMap(argument, sandbox.files);
                sandbox.getScene().showCubebox = false;
            }
            else
                std::cout << "Argument '" << argument << "' should be followed by a <environmental_map>. Skipping argument." << std::endl;
        }
        else if ( argument == "-C" ) {
            if(++i < argc)
            {
                argument = std::string(argv[i]);
                sandbox.uniforms.setCubeMap(argument, sandbox.files);
                sandbox.getScene().showCubebox = true;
            }
            else
                std::cout << "Argument '" << argument << "' should be followed by a <environmental_map>. Skipping argument." << std::endl;
        }
        else if ( argument.find("-D") == 0 ) {
            // Defines are added/remove once existing shaders
            // On multiple meshes files like OBJ, there can be multiple 
            // variations of meshes, that only get created after loading the sece
            // to work around that defines are add post-loading as argument commands
            std::string define = std::string("define,") + argument.substr(2);
            commandsArgs.push_back(define);
        }
        else if ( argument.find("-I") == 0 ) {
            std::string include = argument.substr(2);
            sandbox.include_folders.push_back(include);
        }
        else if (   argument == "-v" || 
                    argument == "--version") {
            std::cout << version << std::endl;
        }
        else if ( argument.find("-") == 0 ) {
            std::string parameterPair = argument.substr( argument.find_last_of('-') + 1 );
            if(++i < argc) {
                argument = std::string(argv[i]);

                // If it's a video file, capture device, streaming url or Image sequence
                if (ada::haveExt(argument,"mov") || ada::haveExt(argument,"MOV") ||
                    ada::haveExt(argument,"mp4") || ada::haveExt(argument,"MP4") ||
                    ada::haveExt(argument,"mpeg") || ada::haveExt(argument,"MPEG") ||
                    argument.rfind("/dev/", 0) == 0 ||
                    argument.rfind("http://", 0) == 0 ||
                    argument.rfind("https://", 0) == 0 ||
                    argument.rfind("rtsp://", 0) == 0 ||
                    argument.rfind("rtmp://", 0) == 0 ||
                    ada::check_for_pattern(argument) ) {
                    sandbox.uniforms.addStreamingTexture(parameterPair, argument, sandbox.vFlip, false);
                }
                // Else load it as a single texture
                else 
                    sandbox.uniforms.addTexture(parameterPair, argument, sandbox.files, sandbox.vFlip);
            }
            else
                std::cout << "Argument '" << argument << "' should be followed by a <texture>. Skipping argument." << std::endl;
        }
        else {

            if ( sandbox.frag_index == -1 && (ada::haveExt(argument,"frag") || ada::haveExt(argument,"fs") ) ) {
                if ( stat(argument.c_str(), &st) != 0 ) {
                    std::cout << "File " << argument << " not founded. Creating a default fragment shader with that name"<< std::endl;

                    std::ofstream out(argument);
                    if (willLoadGeometry)
                        out << ada::getDefaultSrc(ada::FRAG_DEFAULT_SCENE);
                    else if (willLoadTextures)
                        out << ada::getDefaultSrc(ada::FRAG_DEFAULT_TEXTURE);
                    else 
                        out << ada::getDefaultSrc(ada::FRAG_DEFAULT);
                    out.close();
                }
            }
            else if ( sandbox.vert_index == -1 && ( ada::haveExt(argument,"vert") || ada::haveExt(argument,"vs") ) ) {
                if ( stat(argument.c_str(), &st) != 0 ) {
                    std::cout << "File " << argument << " not founded. Creating a default vertex shader with that name"<< std::endl;

                    std::ofstream out(argument);
                    out << ada::getDefaultSrc(ada::VERT_DEFAULT_SCENE);
                    out.close();
                }
            }

            sandbox.loadFile(argument);
        } 
    }

    if (sandbox.verbose) {
        printf("//Specs: \n");
        printf("//  - Vendor: %s\n", ada::getVendor().c_str() );
        printf("//  - Renderer: %s\n", ada::getRenderer().c_str() );
        printf("//  - Version: %s\n", ada::getGLVersion().c_str() );
        printf("//  - GLSL version: %s\n", ada::getGLSLVersion().c_str() );
        printf("//  - Extensions: %s\n", ada::getExtensions().c_str() );

        printf("//  - Implementation limits:\n");
        int param;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &param);
        std::cout << "//      + GL_MAX_TEXTURE_SIZE = " << param << std::endl;

    }

    // If no shader
    #ifndef __EMSCRIPTEN__
    if ( sandbox.frag_index == -1 && sandbox.vert_index == -1 && sandbox.geom_index == -1 ) {
        printUsage(argv[0]);
        onExit();
        exit(EXIT_FAILURE);
    }
    #endif

    sandbox.init();

#ifdef __EMSCRIPTEN__
    emscripten_request_animation_frame_loop(loop, 0);

    double width,  height;
    emscripten_get_element_css_size("#canvas", &width, &height);
    ada::setWindowSize(width, height);

#else

    ada::setWindowVSync(true);

    // Start watchers
    fileChanged = -1;
    std::thread fileWatcher( &fileWatcherThread );
    std::thread cinWatcher( &cinWatcherThread );

    // OSC
    #if defined(SUPPORT_OSC)
    lo::ServerThread oscServer(oscPort);
    oscServer.set_callbacks( [&st](){printf("// Listening for OSC commands on port: %i\n", oscPort);}, [](){});
    oscServer.add_method(0, 0, [](const char *path, lo::Message m) {
        std::string line;
        std::vector<std::string> address = ada::split(std::string(path), '/');
        for (size_t i = 0; i < address.size(); i++)
            line +=  ((i != 0) ? "," : "") + address[i];

        std::string types = m.types();
        lo_arg** argv = m.argv(); 
        lo_message msg = m;
        for (size_t i = 0; i < types.size(); i++) {
            if ( types[i] == 's')
                line += "," + std::string( (const char*)argv[i] );
            else if (types[i] == 'i')
                line += "," + ada::toString(argv[i]->i);
            else
                line += "," + ada::toString(argv[i]->f);
        }

        if (sandbox.verbose)
            std::cout << line << std::endl;
            
        sandbox.commandsRun(line, oscMutex);
    });

    if (oscPort > 0) {
        oscServer.start();
    }
    #endif
    
    // Render Loop
    while ( ada::isGL() && keepRunnig.load() ){
        // Something change??
        if ( fileChanged != -1 ) {
            sandbox.onFileChange( fileChanged );
            fileChanged = -1;
        }

        loop();
    }

    
    // If is terminated by the windows manager, turn keepRunnig off so the fileWatcher can stop
    if ( !ada::isGL() )
        keepRunnig.store(false);

    onExit();
    
    // Wait for watchers to end
    fileWatcher.join();

    // Force cinWatcher to finish (because is waiting for input)
    #ifndef PLATFORM_WINDOWS
    pthread_t cinHandler = cinWatcher.native_handle();
    pthread_cancel( cinHandler );
    #endif//
    exit(0);
#endif

    return 1;
}

// Events
//============================================================================
void ada::onKeyPress (int _key) {
    if (screensaver) {
        keepRunnig = false;
        keepRunnig.store(false);
    }
    else {
        if (_key == 'q' || _key == 'Q') {
            keepRunnig = false;
            keepRunnig.store(false);
        }
    }
}

void ada::onMouseMove(float _x, float _y) {
    if (screensaver) {
        if (sandbox.isReady()) {
            keepRunnig = false;
            keepRunnig.store(false);
        }
    }
}

void ada::onMouseClick(float _x, float _y, int _button) { }
void ada::onScroll(float _yoffset) { sandbox.onScroll(_yoffset); }
void ada::onMouseDrag(float _x, float _y, int _button) { sandbox.onMouseDrag(_x, _y, _button); }
void ada::onViewportResize(int _newWidth, int _newHeight) { sandbox.onViewportResize(_newWidth, _newHeight); }

void commandsInit() {

    sandbox.commands.push_back( Command("define,", [&](const std::string& _line){ 
        std::vector<std::string> values = ada::split(_line,',');
        bool change = false;
        if (values.size() == 2) {
            std::vector<std::string> v = ada::split(values[1],' ');
            if (v.size() > 1)
                sandbox.addDefine( v[0], v[1] );
            else
                sandbox.addDefine( v[0] );
            change = true;
        }
        else if (values.size() == 3) {
            sandbox.addDefine( values[1], values[2] );
            change = true;
        }

        if (change) {
            fullFps = true;
            for (size_t i = 0; i < sandbox.files.size(); i++) {
                if (sandbox.files[i].type == FRAG_SHADER ||
                    sandbox.files[i].type == VERT_SHADER ) {
                        sandbox.filesMutex.lock();
                        fileChanged = i;
                        sandbox.filesMutex.unlock();
                        std::this_thread::sleep_for(std::chrono::milliseconds( ada::getRestMs() ));
                }
            }
            fullFps = false;
        }
        return change;
    },
    "define,<KEYWORD>               add a define to the shader", false));

    sandbox.commands.push_back( Command("undefine,", [&](const std::string& _line){ 
        std::vector<std::string> values = ada::split(_line,',');
        if (values.size() == 2) {
            sandbox.delDefine( values[1] );
            fullFps = true;
            for (size_t i = 0; i < sandbox.files.size(); i++) {
                if (sandbox.files[i].type == FRAG_SHADER ||
                    sandbox.files[i].type == VERT_SHADER ) {
                        sandbox.filesMutex.lock();
                        fileChanged = i;
                        sandbox.filesMutex.unlock();
                        std::this_thread::sleep_for(std::chrono::milliseconds( ada::getRestMs() ));
                }
            }
            fullFps = false;
            return true;
        }
        return false;
    },
    "undefine,<KEYWORD>             remove a define on the shader", false));

    sandbox.commands.push_back(Command("reload", [&](const std::string& _line){ 
        if (_line == "reload" || _line == "reload,all") {
            fullFps = true;
            for (size_t i = 0; i < sandbox.files.size(); i++) {
                sandbox.filesMutex.lock();
                fileChanged = i;
                sandbox.filesMutex.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds( ada::getRestMs() ));
            }
            fullFps = false;
            return true;
        }
        else {
            std::vector<std::string> values = ada::split(_line,',');
            if (values.size() == 2 && values[0] == "reload") {
                for (size_t i = 0; i < sandbox.files.size(); i++) {
                    if (sandbox.files[i].path == values[1]) {
                        sandbox.filesMutex.lock();
                        fileChanged = i;
                        sandbox.filesMutex.unlock();
                        return true;
                    } 
                }
            }
        }
        return false;
    },
    "reload[,<filename>]            reload one or all files", false));

    sandbox.commands.push_back(Command("version", [&](const std::string& _line){ 
        if (_line == "version") {
            std::cout << version << std::endl;
            return true;
        }
        return false;
    },
    "version                        return glslViewer version.", false));

    sandbox.commands.push_back(Command("window_width", [&](const std::string& _line){ 
        if (_line == "window_width") {
            std::cout << ada::getWindowWidth() << std::endl;
            return true;
        }
        return false;
    },
    "window_width                   return the width of the windows.", false));

    sandbox.commands.push_back(Command("window_height", [&](const std::string& _line){ 
        if (_line == "window_height") {
            std::cout << ada::getWindowHeight() << std::endl;
            return true;
        }
        return false;
    },
    "window_height                  return the height of the windows.", false));

    sandbox.commands.push_back(Command("pixel_density", [&](const std::string& _line){ 
        if (_line == "pixel_density") {
            std::cout << ada::getPixelDensity() << std::endl;
            return true;
        }
        return false;
    },
    "pixel_density                  return the pixel density.", false));

    sandbox.commands.push_back(Command("screen_size", [&](const std::string& _line){ 
        if (_line == "screen_size") {
            glm::ivec2 screen_size = ada::getScreenSize();
            std::cout << screen_size.x << ',' << screen_size.y << std::endl;
            return true;
        }
        return false;
    },
    "screen_size                    return the screen size.", false));

    sandbox.commands.push_back(Command("viewport", [&](const std::string& _line){ 
        if (_line == "viewport") {
            glm::ivec4 viewport = ada::getViewport();
            std::cout << viewport.x << ',' << viewport.y << ',' << viewport.z << ',' << viewport.w << std::endl;
            return true;
        }
        return false;
    },
    "viewport                       return the viewport size.", false));

    sandbox.commands.push_back(Command("mouse", [&](const std::string& _line){ 
        if (_line == "mouse") {
            glm::vec2 pos = ada::getMousePosition();
            std::cout << pos.x << "," << pos.y << std::endl;
            return true;
        }
        return false;
    },
    "mouse                          return the mouse position.", false));
    
    sandbox.commands.push_back(Command("fps", [&](const std::string& _line){
        std::vector<std::string> values = ada::split(_line,',');
        if (values.size() == 2) {
            // commandsMutex.lock();
            ada::setFps( ada::toInt(values[1]) );
            // commandsMutex.unlock();
            return true;
        }
        else {
            // Force the output in floats
            printf("%f\n", ada::getFps());
            return true;
        }
        return false;
    },
    "fps                            return or set the amount of frames per second.", false));

    sandbox.commands.push_back(Command("delta", [&](const std::string& _line){ 
        if (_line == "delta") {
            // Force the output in floats
            printf("%f\n", ada::getDelta());
            return true;
        }
        return false;
    },
    "delta                          return u_delta, the secs between frames.", false));

    sandbox.commands.push_back(Command("date", [&](const std::string& _line){ 
        if (_line == "date") {
            // Force the output in floats
            glm::vec4 date = ada::getDate();
            std::cout << date.x << ',' << date.y << ',' << date.z << ',' << date.w << std::endl;
            return true;
        }
        return false;
    },
    "date                           return u_date as YYYY, M, D and Secs.", false));

    sandbox.commands.push_back(Command("fullFps", [&](const std::string& _line){
        if (_line == "fullFps") {
            std::string rta = fullFps ? "on" : "off";
            std::cout <<  rta << std::endl; 
            return true;
        }
        else {
            std::vector<std::string> values = ada::split(_line,',');
            if (values.size() == 2) {
                // sandbox.commandsMutex.lock();
                fullFps = (values[1] == "on");
                // sandbox.commandsMutex.unlock();
            }
        }
        return false;
    },
    "fullFps[,on|off]               go to full FPS or not", false));

    sandbox.commands.push_back(Command("q", [&](const std::string& _line){ 
        if (_line == "q") {
            keepRunnig.store(false);
            return true;
        }
        return false;
    },
    "q                              close glslViewer", false));

    sandbox.commands.push_back(Command("quit", [&](const std::string& _line){ 
        if (_line == "quit") {
            bTerminate = true;
            // keepRunnig.store(false);
            return true;
        }
        return false;
    },
    "quit                           close glslViewer", false));

    sandbox.commands.push_back(Command("exit", [&](const std::string& _line){ 
        if (_line == "exit") {
            bTerminate = true;
            // keepRunnig.store(false);
            return true;
        }
        return false;
    },
    "exit                           close glslViewer", false));
}

#ifndef __EMSCRIPTEN__

void printUsage(char * executableName) {
    std::cerr << "// " << header << std::endl;
    std::cerr << "// "<< std::endl;
    std::cerr << "// Swiss army knife of GLSL Shaders. Loads frag/vertex shaders, images, " << std::endl;
    std::cerr << "// videos, audio, geometries and much more. Your assets will reload  "<< std::endl;
    std::cerr << "// automatically on changes. It have support for multiple buffers,  "<< std::endl;
    std::cerr << "// background and postprocessing passes. It can render headlessly into"<< std::endl;
    std::cerr << "// a file, a PNG sequence, or fullscreen, as a screensaver, live mode (allways "<< std::endl;
    std::cerr << "// on top) or to volumetric displays. "<< std::endl;
    std::cerr << "// Use POSIX STANDARD CONSOLE IN/OUT to comunicate (uniforms, camera "<< std::endl;
    std::cerr << "// properties, lights and other scene properties) to and with other "<< std::endl;
    std::cerr << "// programs. Compatible with Linux, MacOS and Windows, runs from "<< std::endl;
    std::cerr << "// command line with or outside X11 environment on RaspberryPi devices. "<< std::endl;
    std::cerr << "// "<< std::endl;
    std::cerr << "// For more information refer to repository:"<< std::endl;
    std::cerr << "// https://github.com/patriciogonzalezvivo/glslViewer"<< std::endl;
    std::cerr << "// or joing the #glslViewer channel in https://shader.zone/"<< std::endl;
    std::cerr << "// "<< std::endl;
    std::cerr << "// Usage: " << executableName << " [Arguments]"<< std::endl;
    std::cerr << "// "<< std::endl;
    std::cerr << "// Arguments:" << std::endl;
    std::cerr << "// <shader>.frag [<shader>.vert] - load shaders" << std::endl;
    std::cerr << "// [<mesh>.(obj/ply/stl/glb/gltf)] - load obj/ply/stl/glb/gltf file" << std::endl;
    std::cerr << "// [<texture>.(png/tga/jpg/bmp/psd/gif/hdr/mov/mp4/rtsp/rtmp/etc)] - load and assign texture to uniform order" << std::endl;
    std::cerr << "// [-vFlip] - all textures after will be flipped vertically" << std::endl;
    std::cerr << "// [--video <video_device_number>] - open video device allocated wit that particular id" << std::endl;
    std::cerr << "// [--audio <capture_device_id>] - open audio capture device allocated as sampler2D texture. If id is not selected, default will be used" << std::endl;
    std::cerr << "// [-<uniformName> <texture>.(png/tga/jpg/bmp/psd/gif/hdr)] - add textures associated with different uniform sampler2D names" << std::endl;
    std::cerr << "// [-C <enviromental_map>.(png/tga/jpg/bmp/psd/gif/hdr)] - load a environmental map as cubemap" << std::endl;
    std::cerr << "// [-c <enviromental_map>.(png/tga/jpg/bmp/psd/gif/hdr)] - load a environmental map as cubemap but hided" << std::endl;
    std::cerr << "// [-sh <enviromental_map>.(png/tga/jpg/bmp/psd/gif/hdr)] - load a environmental map as spherical harmonics array" << std::endl;
    std::cerr << "// [-x <pixels>] - set the X position of the billboard on the screen" << std::endl;
    std::cerr << "// [-y <pixels>] - set the Y position of the billboard on the screen" << std::endl;
    std::cerr << "// [-w <pixels>] - set the width of the window" << std::endl;
    std::cerr << "// [-h <pixels>] - set the height of the billboard" << std::endl;
    std::cerr << "// [--fps] <fps> - fix the max FPS" << std::endl;
    std::cerr << "// [-f|--fullscreen] - load the window in fullscreen" << std::endl;
    std::cerr << "// [-l|--life-coding] - live code mode, where the billboard is allways visible" << std::endl;
    std::cerr << "// [-ss|--screensaver] - screensaver mode, any pressed key will exit" << std::endl;
    std::cerr << "// [--headless] - headless rendering. Very useful for making images or benchmarking." << std::endl;
    std::cerr << "// [--nocursor] - hide cursor" << std::endl;
    std::cerr << "// [--fxaa] - set FXAA as postprocess filter" << std::endl;
    std::cerr << "// [--holoplay <[0..7]>] - HoloPlay volumetric postprocess (Looking Glass Model)" << std::endl;
    std::cerr << "// [-I<include_folder>] - add an include folder to default for #include files" << std::endl;
    std::cerr << "// [-D<define>] - add system #defines directly from the console argument" << std::endl;
    std::cerr << "// [-p <osc_port>] - open OSC listening port" << std::endl;
    std::cerr << "// [-e/-E <command>] - execute command when start. Multiple -e flags can be chained" << std::endl;
    std::cerr << "// [-v/--version] - return glslViewer version" << std::endl;
    std::cerr << "// [--verbose] - turn verbose outputs on" << std::endl;
    std::cerr << "// [--help] - print help for one or all command" << std::endl;
}

void onExit() {
    // clear screen
    glClear( GL_COLOR_BUFFER_BIT );

    // Delete the resources of Sandbox
    sandbox.clear();

    // close openGL instance
    ada::closeGL();
}

//  Watching Thread
//============================================================================
void fileWatcherThread() {
    struct stat st;
    while ( keepRunnig.load() ) {
        for ( uint32_t i = 0; i < sandbox.files.size(); i++ ) {
            if ( fileChanged == -1 ) {
                stat(  sandbox.files[i].path.c_str(), &st );
                int date = st.st_mtime;
                if ( date !=  sandbox.files[i].lastChange ) {
                    // filesMutex.lock();
                     sandbox.files[i].lastChange = date;
                    fileChanged = i;
                    // filesMutex.unlock();
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds( 500 ));
    }
}

//  Command line Thread
//============================================================================
void cinWatcherThread() {
    while (!sandbox.isReady()) {
        ada::sleep_ms( ada::getRestSec() * 1000000 );
        std::this_thread::sleep_for(std::chrono::milliseconds( ada::getRestMs() ));
    }

    // Argument commands to execute comming from -e or -E
    if (commandsArgs.size() > 0) {
        for (size_t i = 0; i < commandsArgs.size(); i++) {
            sandbox.commandsRun(commandsArgs[i]);
        }
        commandsArgs.clear();

        // If it's using -E exit after executing all commands
        if (commandsExit) {
            bTerminate = true;
            // keepRunnig.store(false);
        }
    }

    // Commands comming from the console IN
    std::string console_line;
    std::cout << "// > ";
    while (std::getline(std::cin, console_line)) {
        sandbox.commandsRun(console_line);
        std::cout << "// > ";
    }
}

#endif