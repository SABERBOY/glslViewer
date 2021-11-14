#pragma once

#include <map>
#include <vector>
#include <string>
#include <functional>

#include "ada/gl/fbo.h"
#include "ada/gl/shader.h"
#include "ada/gl/texture.h"
#include "ada/gl/textureStream.h"
#include "ada/gl/convolutionPyramid.h"
#include "ada/tools/fs.h"
#include "ada/scene/camera.h"
#include "ada/scene/light.h"

#include "gl/textureAudio.h"
#include "types/files.h"

struct UniformData {
    std::string getType();

    float   value[4];
    int     size;
    bool    bInt = false;
    bool    change = false;
};

typedef std::map<std::string, UniformData> UniformDataList;

struct UniformFunction {
    UniformFunction();
    UniformFunction(const std::string &_type);
    UniformFunction(const std::string &_type, std::function<void(ada::Shader&)> _assign);
    UniformFunction(const std::string &_type, std::function<void(ada::Shader&)> _assign, std::function<std::string()> _print);

    std::function<void(ada::Shader&)> assign;
    std::function<std::string()>    print;
    std::string                     type;
    bool                            present = false;
};

typedef std::map<std::string, UniformFunction> UniformFunctionsList;
typedef std::map<std::string, ada::Texture*> TextureList;
typedef std::map<std::string, ada::TextureStream*> StreamsList;

struct CameraData {
    glm::vec3   position;
    glm::vec3   right;
    glm::vec3   up;
    glm::vec3   forward;
    float       minDepth;
    float       maxDepth;
    float       fovHorz;
    float       fovVert;

    float       tanHFov2;
    float       tanVFov2;
};

class Uniforms {
public:
    Uniforms();
    virtual ~Uniforms();

    // Ingest new uniforms
    bool                    parseLine( const std::string &_line );

    bool                    addTexture( const std::string& _name, ada::Texture* _texture );
    bool                    addTexture( const std::string& _name, const std::string& _path, WatchFileList& _files, bool _flip = true, bool _verbose = true );
    bool                    addBumpTexture( const std::string& _name, const std::string& _path, WatchFileList& _files, bool _flip = true, bool _verbose = true );
    bool                    addStreamingTexture( const std::string& _name, const std::string& _url, bool _flip = true, bool _device = false, bool _verbose = true );
    bool                    addAudioTexture( const std::string& _name, const std::string& device_id, bool _flip = false, bool _verbose = true );
    bool                    addCameraTrack( const std::string& _name );
    void                    update(unsigned int _frame);

    void                    set( const std::string& _name, float _value);
    void                    set( const std::string& _name, float _x, float _y);
    void                    set( const std::string& _name, float _x, float _y, float _z);
    void                    set( const std::string& _name, float _x, float _y, float _z, float _w);
    
    void                    setCubeMap( ada::TextureCube* _cubemap );
    void                    setCubeMap( const std::string& _filename, WatchFileList& _files, bool _verbose = true);

    // Check presence of uniforms on shaders
    void                    checkPresenceIn( const std::string &_vert_src, const std::string &_frag_src );

    // Feed uniforms to a specific shader
    bool                    feedTo( ada::Shader &_shader, bool _lights = true );
    bool                    feedTo( ada::Shader *_shader, bool _lights = true );

    ada::Camera&            getCamera() { return cameras[0]; }

    // Debug
    void                    print(bool _all);
    void                    printBuffers();
    void                    printTextures();
    void                    printLights();

    // Change state
    void                    flagChange();
    void                    unflagChange();
    bool                    haveChange();

    void                    clear();

    // Manually defined uniforms (through console IN)
    UniformDataList         data;

    // Automatic uniforms
    UniformFunctionsList    functions;

    // Common 
    TextureList             textures;
    StreamsList             streams;

    ada::TextureCube*       cubemap;
    std::vector<ada::Fbo>   buffers;
    std::vector<ada::ConvolutionPyramid> convolution_pyramids;

    // 3d Scene Uniforms 
    std::vector<ada::Light>     lights;
    std::vector<ada::Camera>    cameras;
    std::vector<CameraData>     cameraTrack;

protected:
    bool                    m_change;
    bool                    m_is_audio_init;
};


