cmake_minimum_required(VERSION 3.7)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(Rush VERSION 0.1.0.0)

set(RUSH_RENDER_API "NULL" CACHE STRING "Select renderer type")

# NOTE: only Vulkan renderer is currently supported
set_property(CACHE RUSH_RENDER_API PROPERTY STRINGS
	NULL
	VK
)

# Add source files

set(RUSH_SRC
	Rush/GfxBitmapFont.cpp
	Rush/GfxBitmapFont.h
	Rush/GfxCommon.cpp
	Rush/GfxCommon.h
	Rush/GfxDevice.h
	Rush/GfxDeviceVK.cpp
	Rush/GfxDeviceVK.h
	Rush/GfxEmbeddedShaders.cpp
	Rush/GfxEmbeddedShaders.h
	Rush/GfxPrimitiveBatch.cpp
	Rush/GfxPrimitiveBatch.h
	Rush/GfxRef.h
	Rush/MathCommon.h
	Rush/MathTypes.cpp
	Rush/MathTypes.h
	Rush/Platform.cpp
	Rush/Platform.h
	Rush/PlatformWin32.cpp
	Rush/PlatformLinux.cpp
	Rush/Rush.h
	Rush/UtilCamera.cpp
	Rush/UtilCamera.h
	Rush/UtilCameraManipulator.cpp
	Rush/UtilCameraManipulator.h
	Rush/UtilColor.h
	Rush/UtilDataStream.h
	Rush/UtilFile.cpp
	Rush/UtilFile.h
	Rush/UtilLinearAllocator.h
	Rush/UtilLog.cpp
	Rush/UtilLog.h
	Rush/UtilRandom.h
	Rush/UtilResourcePool.h
	Rush/UtilStaticArray.h
	Rush/UtilTimer.cpp
	Rush/UtilTimer.h
	Rush/UtilTuple.h
	Rush/Window.cpp
	Rush/Window.h
	Rush/WindowWin32.cpp
	Rush/WindowWin32.h
	Rush/WindowXCB.cpp
	Rush/WindowXCB.h
)

add_library(Rush STATIC ${RUSH_SRC})
target_include_directories(Rush INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(Rush PUBLIC cxx_std_14)

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_compile_definitions(Rush PUBLIC
		RUSH_PLATFORM_WINDOWS
	)
	target_compile_definitions(Rush PRIVATE
		NOMINMAX
		WIN32_LEAN_AND_MEAN
	)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	target_compile_definitions(Rush PUBLIC
		RUSH_PLATFORM_LINUX
	)
	# Should try to detect XCB support, but assume it exists in standard locations for now...
	# find_package(XCB REQUIRED)
	# target_include_directories(Rush PRIVATE ${XCB_INCLUDE_DIRS})
	# target_link_libraries(Rush INTERFACE ${XCB_LIBRARIES})
	target_link_libraries(Rush INTERFACE xcb xcb-keysyms)
endif()

if (${RUSH_RENDER_API} MATCHES "NULL")
	target_compile_definitions(Rush PUBLIC
		RUSH_RENDER_API=RUSH_RENDER_API_NULL
	)
endif()

if (${RUSH_RENDER_API} MATCHES "VK")
	target_compile_definitions(Rush PUBLIC
		RUSH_RENDER_API=RUSH_RENDER_API_VK
	)
	find_package(Vulkan REQUIRED)
	target_link_libraries(Rush INTERFACE
		${Vulkan_LIBRARIES}
	)
	target_include_directories(Rush PRIVATE
		${Vulkan_INCLUDE_DIRS}
	)
endif()

source_group("" FILES ${RUSH_SRC})
