cmake_minimum_required(VERSION 3.16)

project(QRTWhisper VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC OFF) # No usamos archivos .ui

set (WHISPER_PATH "${CMAKE_SOURCE_DIR}/extern/whisper.cpp")
set(WHISPER_EXAMPLES "${WHISPER_PATH}/examples")

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# Backend de micrófono virtual: 'libpulse' (API C, default) o 'pactl' (QProcess).
set(QRTWHISPER_MIC_BACKEND "libpulse" CACHE STRING "Backend de microfono virtual: libpulse o pactl")
set_property(CACHE QRTWHISPER_MIC_BACKEND PROPERTY STRINGS libpulse pactl)

set(QRTWHISPER_MIC_SOURCES "")
set(QRTWHISPER_MIC_DEFINES "")
set(QRTWHISPER_MIC_LINK "")
if(QRTWHISPER_MIC_BACKEND STREQUAL "libpulse")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBPULSE REQUIRED IMPORTED_TARGET libpulse)
    set(QRTWHISPER_MIC_SOURCES model/PulseAudioVirtualMic.cpp model/PulseAudioVirtualMic.h)
    set(QRTWHISPER_MIC_DEFINES QRTWHISPER_MIC_BACKEND_LIBPULSE)
    set(QRTWHISPER_MIC_LINK PkgConfig::LIBPULSE)
elseif(QRTWHISPER_MIC_BACKEND STREQUAL "pactl")
    set(QRTWHISPER_MIC_SOURCES model/PactlVirtualMic.cpp model/PactlVirtualMic.h)
    set(QRTWHISPER_MIC_DEFINES QRTWHISPER_MIC_BACKEND_PACTL)
else()
    message(FATAL_ERROR "QRTWHISPER_MIC_BACKEND invalido: '${QRTWHISPER_MIC_BACKEND}'. Valores validos: libpulse, pactl")
endif()

add_executable(${PROJECT_NAME}
        main.cpp
        model/model.cpp model/model.h
        model/WhisperParams.h
        model/TranscriptionWorker.cpp model/TranscriptionWorker.h
        model/audio/IAudioStream.h
        model/audio/SdlAudioStream.cpp model/audio/SdlAudioStream.h
        model/engine/ITranscriptionEngine.h
        model/engine/WhisperEngine.cpp model/engine/WhisperEngine.h
        controller/controller.cpp controller/controller.h
        view/TextRender.cpp view/TextRender.h
        view/TransparentWindow.h
        view/View.cpp view/View.h
        view/MainWidget.cpp view/MainWidget.h
        view/Tray.cpp view/Tray.h
        model/VirtualMic.h
        model/VirtualMicFactory.cpp model/VirtualMicFactory.h
        ${QRTWHISPER_MIC_SOURCES}
        model/ModelManager.cpp model/ModelManager.h
        view/ModelManagerDialog.cpp view/ModelManagerDialog.h
        resources/resources.qrc
        translations/translations.qrc
)

add_executable(sdl_mics tools/sdl_mics.cpp)

set(GGML_CUDA 1)
# Arquitecturas CUDA a compilar (fat binary): el driver elige en runtime la que
# coincida con la GPU presente; un solo binario corre en varias GPUs.
#  75 = Turing  (GTX 16xx, p.ej. GTX 1660 SUPER)
#  86 = Ampere  (RTX 30xx)
#  89 = Ada     (RTX 40xx)
# 120 = Blackwell (RTX 50xx) - requiere CUDA >= 12.8; con CUDA menor corre via JIT de PTX (compute_89).
# Sobreescribir con -DCMAKE_CUDA_ARCHITECTURES="..." si se quiere fijar; 'native' (CMake>=3.23) = solo GPU de build.
# Default: solo 75 (Turing, GTX 16xx) para compilaciones rapidas durante desarrollo.
# Para release con soporte multi-GPU, sobreescribir en configure:
#   -DCMAKE_CUDA_ARCHITECTURES=75;86;89
#   -DCMAKE_CUDA_ARCHITECTURES="75;86;89;120" (RTX 50xx, requiere CUDA >= 12.8)
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES OR CMAKE_CUDA_ARCHITECTURES STREQUAL "")
    set(CMAKE_CUDA_ARCHITECTURES "75" CACHE STRING "Arquitecturas CUDA a compilar (75 = GTX 16xx)")
    message(STATUS "QRTWhisper: CUDA architectures default: 75")
endif()
set(WHISPER_SDL2 ON)

#uncomment for hardcode the path to cuda compiler
#set(CMAKE_CUDA_COMPILER /usr/local/cuda-12.9/bin/nvcc)

add_subdirectory(extern/whisper.cpp)


add_library(common
        ${WHISPER_EXAMPLES}/common.h
        ${WHISPER_EXAMPLES}/common.cpp
        ${WHISPER_EXAMPLES}/common-sdl.h
        ${WHISPER_EXAMPLES}/common-sdl.cpp

)
target_include_directories(${PROJECT_NAME} PRIVATE ${WHISPER_EXAMPLES})
target_include_directories(${PROJECT_NAME} PRIVATE /usr/include/SDL2)
target_include_directories(common PRIVATE /usr/include/SDL2)
target_include_directories(${PROJECT_NAME} PRIVATE "${WHISPER_PATH}/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${WHISPER_PATH}/ggml/include")
target_include_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/model"
        "${CMAKE_CURRENT_SOURCE_DIR}/view"
        "${CMAKE_CURRENT_SOURCE_DIR}/controller"
)
target_include_directories(sdl_mics PRIVATE /usr/include/SDL2)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Network)
target_link_libraries(${PROJECT_NAME} PRIVATE whisper)
target_link_libraries(${PROJECT_NAME} PRIVATE common)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2)
if(QRTWHISPER_MIC_LINK)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${QRTWHISPER_MIC_LINK})
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE ${QRTWHISPER_MIC_DEFINES})

target_link_libraries(sdl_mics PRIVATE SDL2)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
# Icono de la aplicacion en rutas estandar (desktop environments / Flatpak / instalacion manual)
install(FILES resources/icon.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME QRTWhisper.png
)
install(FILES resources/icon.png
        DESTINATION share/pixmaps
        RENAME QRTWhisper.png
)

# Habilitar CPack para generar .deb
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "qrtwhisper")
set(CPACK_PACKAGE_VERSION "1.0")
set(CPACK_PACKAGE_DESCRIPTION "Aplicación con SDL2 y CUDA")
set(CPACK_PACKAGE_CONTACT "tucorreo@example.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Tu Nombre")
set(CPACK_PACKAGE_VENDOR "TuEmpresa")

# Dependencias del paquete (.deb)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0, cuda-runtime-12-9")

# Generador DEB (requiere 'dpkg' instalado)
set(CPACK_GENERATOR "DEB")

include(CPack)
