cmake_minimum_required(VERSION 3.7)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project(Rush VERSION 0.1.0.0)

set(RUSH_RENDER_API "NULL" CACHE STRING "Select renderer type")

option(RUSH_CLANG_TIDY "Run clang-tidy static analysis" OFF)

if (RUSH_CLANG_TIDY)
	set(RUSH_CLANG_TIDY_CHECKS
		*
		-*avoid-c-arrays
		-*braces-around-statements
		-*deprecated-headers
		-*magic-numbers
		-*uppercase-literal-suffix
		-*use-auto
		-cppcoreguidelines*
		-fuchsia*
		-google-readability-casting
		-google-readability-todo
		-hicpp*
		-misc-non-private-member-variables-in-classes
		-readability-implicit-bool-conversion
		hicpp-member-init
	)
	string(REPLACE ";" "," RUSH_CLANG_TIDY_CHECKS "${RUSH_CLANG_TIDY_CHECKS}")
	set(CMAKE_CXX_CLANG_TIDY clang-tidy; -checks=${RUSH_CLANG_TIDY_CHECKS})
endif()

# NOTE: only Vulkan renderer is currently supported
set_property(CACHE RUSH_RENDER_API PROPERTY STRINGS
	NULL
	VK
	MTL
)

add_library(Rush STATIC
	Rush/GfxBitmapFont.cpp
	Rush/GfxBitmapFont.h
	Rush/GfxCommon.cpp
	Rush/GfxCommon.h
	Rush/GfxDevice.h
	Rush/GfxDeviceVK.cpp
	Rush/GfxDeviceVK.h
	Rush/GfxEmbeddedShaders.cpp
	Rush/GfxEmbeddedShaders.h
	Rush/GfxEmbeddedShadersMSL.cpp
	Rush/GfxPrimitiveBatch.cpp
	Rush/GfxPrimitiveBatch.h
	Rush/MathCommon.h
	Rush/MathTypes.cpp
	Rush/MathTypes.h
	Rush/Platform.cpp
	Rush/Platform.h
	Rush/PlatformLinux.cpp
	Rush/PlatformWin32.cpp
	Rush/Rush.h
	Rush/UtilArray.h
	Rush/UtilBuffer.h
	Rush/UtilCamera.cpp
	Rush/UtilCamera.h
	Rush/UtilColor.h
	Rush/UtilDataStream.h
	Rush/UtilFile.cpp
	Rush/UtilFile.h
	Rush/UtilHash.h
	Rush/UtilLinearAllocator.h
	Rush/UtilLog.cpp
	Rush/UtilLog.h
	Rush/UtilMemory.h
	Rush/UtilRandom.h
	Rush/UtilResourcePool.h
	Rush/UtilString.h
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

	set(RUSH_VULKAN_PLATFORM VK_USE_PLATFORM_WIN32_KHR)

elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

	target_compile_definitions(Rush PUBLIC
		RUSH_PLATFORM_LINUX
	)
	# Should try to detect XCB support, but assume it exists in standard locations for now...
	# find_package(XCB REQUIRED)
	# target_include_directories(Rush PRIVATE ${XCB_INCLUDE_DIRS})
	# target_link_libraries(Rush INTERFACE ${XCB_LIBRARIES})
	target_link_libraries(Rush INTERFACE xcb xcb-keysyms pthread dl)

	set(RUSH_VULKAN_PLATFORM VK_USE_PLATFORM_XCB_KHR)

elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

	target_compile_definitions(Rush PUBLIC
		RUSH_PLATFORM_MAC
	)
	target_sources(Rush PRIVATE
		Rush/PlatformMac.mm
		Rush/WindowMac.h
		Rush/WindowMac.mm
	)

	set(RUSH_VULKAN_PLATFORM VK_USE_PLATFORM_MACOS_MVK)

endif()

if (${RUSH_RENDER_API} MATCHES "NULL")
	target_compile_definitions(Rush PUBLIC
		RUSH_RENDER_API=RUSH_RENDER_API_NULL
	)
endif()

if (${RUSH_RENDER_API} MATCHES "VK")
	target_compile_definitions(Rush PUBLIC
		RUSH_RENDER_API=RUSH_RENDER_API_VK
		${RUSH_VULKAN_PLATFORM}
	)
	target_include_directories(Rush PUBLIC
		External/Vulkan-Headers
		External/Volk
	)
	target_sources(Rush PRIVATE
		External/Volk/volk.c
		External/Volk/volk.h
	)
endif()

if (${RUSH_RENDER_API} MATCHES "MTL")
	find_library(Metal_LIBRARIES Metal)
	find_library(AppKit_LIBRARIES AppKit)
	find_library(QuartzCore_LIBRARIES QuartzCore)
	target_link_libraries(Rush INTERFACE 
		${Metal_LIBRARIES}
		${AppKit_LIBRARIES}
		${QuartzCore_LIBRARIES}
	)
	target_compile_definitions(Rush PUBLIC
		RUSH_RENDER_API=RUSH_RENDER_API_MTL
	)
	target_sources(Rush PRIVATE
		Rush/GfxDeviceMtl.h
		Rush/GfxDeviceMtl.mm
	)
endif()

source_group("" FILES ${RUSH_SRC})

if (MSVC)
	target_compile_options(Rush PRIVATE
		-W4
		-WX
		-wd4201 # nonstandard extension used: nameless struct/union
		-wd4100 # unreferenced formal parameter
		-wd4505 # unreferenced local function has been removed
	)
	# target_compile_options(Rush PRIVATE )
	#-WX -w4201 -w4100 -wd4514
else()
	target_compile_options(Rush PRIVATE -Wall)
endif()
