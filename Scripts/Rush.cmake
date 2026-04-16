# Rush.cmake -- turn-key CMake helpers for librush applications
#
# Provides:
#   rush_add_app()           -- create an app target with iOS bundle support
#   rush_shader_hlsl()       -- compile HLSL shaders (Metal or Vulkan)
#   rush_shader_glsl()       -- compile GLSL shaders (Metal or Vulkan)
#   rush_shader_metal()      -- compile native Metal shaders
#   rush_shader_rt()         -- compile ray tracing shaders (Vulkan)

# Tool discovery

set(_RUSH_SDK_BIN_PATHS
	$ENV{VULKAN_SDK}/Bin
	$ENV{VULKAN_SDK}/bin
	$ENV{VULKAN_SDK}/macOS/bin
	$ENV{VK_SDK_PATH}/Bin
	$ENV{VK_SDK_PATH}/bin
	$ENV{VK_SDK_PATH}/macOS/bin
	$ENV{PATH}
	"~/bin"
)

find_program(RUSH_GLSLC NAMES glslc HINTS ${_RUSH_SDK_BIN_PATHS})
find_program(RUSH_DXC NAMES dxc HINTS ${_RUSH_SDK_BIN_PATHS})

if(APPLE)
	if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
		set(RUSH_METAL_SDK "iphonesimulator" CACHE INTERNAL "")
	else()
		set(RUSH_METAL_SDK "macosx" CACHE INTERNAL "")
	endif()

	find_program(RUSH_SPIRV_CROSS NAMES spirv-cross PATHS ${_RUSH_SDK_BIN_PATHS})
	find_program(RUSH_XCRUN NAMES xcrun)
endif()

# rush_add_app(<name>
#     SOURCES <file> ...
#     [LIBS <lib> ...]
#     [BUNDLE_ID <string>]
#     [DEPLOYMENT_TARGET <version>]
#     [DEVICE_FAMILY <string>]
#     [ORIENTATIONS <orient> ...]
# )

function(rush_add_app name)
	cmake_parse_arguments(PARSE_ARGV 1 ARG
		""
		"BUNDLE_ID;DEPLOYMENT_TARGET;DEVICE_FAMILY"
		"SOURCES;LIBS;ORIENTATIONS"
	)

	add_executable(${name} ${ARG_SOURCES})

	if(TARGET Common)
		target_link_libraries(${name} PRIVATE Common)
	else()
		target_link_libraries(${name} PRIVATE Rush)
	endif()

	if(ARG_LIBS)
		target_link_libraries(${name} PRIVATE ${ARG_LIBS})
	endif()

	target_compile_definitions(${name} PRIVATE RUSH_USING_NAMESPACE)

	# Mark shader files so IDE build systems don't try to compile them
	foreach(src IN LISTS ARG_SOURCES)
		get_filename_component(ext "${src}" EXT)
		string(TOLOWER "${ext}" ext)
		if(ext STREQUAL ".hlsl" OR ext STREQUAL ".glsl" OR ext STREQUAL ".metal"
			OR ext STREQUAL ".rgen" OR ext STREQUAL ".rmiss" OR ext STREQUAL ".rchit")
			set_source_files_properties(${src} PROPERTIES HEADER_FILE_ONLY TRUE)
		endif()
	endforeach()

	# iOS bundle configuration
	if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
		# Resolve defaults
		if(NOT ARG_BUNDLE_ID)
			if(RUSH_DEFAULT_BUNDLE_ID)
				set(ARG_BUNDLE_ID "${RUSH_DEFAULT_BUNDLE_ID}")
			else()
				set(ARG_BUNDLE_ID "com.librush.example.app")
			endif()
		endif()

		if(NOT ARG_DEPLOYMENT_TARGET)
			if(RUSH_DEFAULT_DEPLOYMENT_TARGET)
				set(ARG_DEPLOYMENT_TARGET "${RUSH_DEFAULT_DEPLOYMENT_TARGET}")
			else()
				set(ARG_DEPLOYMENT_TARGET "16.0")
			endif()
		endif()

		if(NOT ARG_DEVICE_FAMILY)
			set(ARG_DEVICE_FAMILY "1,2")
		endif()

		if(NOT ARG_ORIENTATIONS)
			set(ARG_ORIENTATIONS
				UIInterfaceOrientationLandscapeLeft
				UIInterfaceOrientationLandscapeRight
			)
		endif()

		# Generate orientation entries for Info.plist
		set(_orientations_xml "")
		foreach(orient IN LISTS ARG_ORIENTATIONS)
			string(APPEND _orientations_xml "\t\t<string>${orient}</string>\n")
		endforeach()

		set(_plist_path "${CMAKE_CURRENT_BINARY_DIR}/${name}_Info.plist.in")
		file(WRITE "${_plist_path}"
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\">
<dict>
\t<key>CFBundleName</key>
\t<string>\${MACOSX_BUNDLE_BUNDLE_NAME}</string>
\t<key>CFBundleIdentifier</key>
\t<string>\${MACOSX_BUNDLE_GUI_IDENTIFIER}</string>
\t<key>CFBundleVersion</key>
\t<string>1.0</string>
\t<key>CFBundleShortVersionString</key>
\t<string>1.0</string>
\t<key>CFBundleExecutable</key>
\t<string>\${MACOSX_BUNDLE_EXECUTABLE_NAME}</string>
\t<key>CFBundlePackageType</key>
\t<string>APPL</string>
\t<key>UILaunchScreen</key>
\t<dict/>
\t<key>UIRequiredDeviceCapabilities</key>
\t<array>
\t\t<string>metal</string>
\t</array>
\t<key>UISupportedInterfaceOrientations</key>
\t<array>
${_orientations_xml}\t</array>
</dict>
</plist>
")

		set_target_properties(${name} PROPERTIES
			MACOSX_BUNDLE TRUE
			MACOSX_BUNDLE_GUI_IDENTIFIER "${ARG_BUNDLE_ID}"
			MACOSX_BUNDLE_BUNDLE_NAME "${name}"
			MACOSX_BUNDLE_INFO_PLIST "${_plist_path}"
			XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "${ARG_DEVICE_FAMILY}"
			XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${ARG_DEPLOYMENT_TARGET}"
		)

		# Copy compiled Metal shaders into app bundle
		add_custom_command(TARGET ${name} POST_BUILD
			COMMAND ${CMAKE_COMMAND}
				-DSRC_DIR=$<TARGET_FILE_DIR:${name}>/..
				-DDST_DIR=$<TARGET_BUNDLE_CONTENT_DIR:${name}>
				-P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CopyMetallibs.cmake
		)
	endif()
endfunction()

# Shader compilation functions

# rush_shader_hlsl(<shader> <profile> [DEPENDS <dep> ...])
function(rush_shader_hlsl shaderName profile)
	cmake_parse_arguments(PARSE_ARGV 2 ARG "" "" "DEPENDS")
	if(NOT RUSH_DXC)
		message(FATAL_ERROR "DXC was not found. Set VULKAN_SDK or VK_SDK_PATH so dxc is available.")
	endif()
	if(APPLE AND DEFINED RUSH_RENDER_API AND RUSH_RENDER_API STREQUAL "MTL")
		if(NOT RUSH_SPIRV_CROSS OR NOT RUSH_XCRUN)
			message(FATAL_ERROR "spirv-cross or xcrun was not found. Ensure spirv-cross is available in PATH for Metal shader compilation.")
		endif()
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.spv ${CMAKE_CFG_INTDIR}/${shaderName}.metallib
			BYPRODUCTS ${CMAKE_CFG_INTDIR}/${shaderName}.metal ${CMAKE_CFG_INTDIR}/${shaderName}.air
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}
			COMMAND ${RUSH_DXC} -spirv -fspv-target-env=vulkan1.2 -fspv-preserve-bindings -T ${profile} -E main -Fo ${CMAKE_CFG_INTDIR}/${shaderName}.spv ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			COMMAND ${RUSH_SPIRV_CROSS} --msl --msl-version 230000 --msl-argument-buffers --msl-decoration-binding --msl-force-active-argument-buffer-resources ${CMAKE_CFG_INTDIR}/${shaderName}.spv > ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			COMMAND ${RUSH_XCRUN} -sdk ${RUSH_METAL_SDK} metal -std=metal3.0 -c ${CMAKE_CFG_INTDIR}/${shaderName}.metal -o ${CMAKE_CFG_INTDIR}/${shaderName}.air
			COMMAND ${RUSH_XCRUN} -sdk ${RUSH_METAL_SDK} metallib ${CMAKE_CFG_INTDIR}/${shaderName}.air -o ${CMAKE_CFG_INTDIR}/${shaderName}.metallib
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			DEPENDS ${ARG_DEPENDS}
		)
	else()
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}
			COMMAND ${RUSH_DXC} -spirv -fspv-target-env=vulkan1.2 -T ${profile} -E main -Fo ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			DEPENDS ${ARG_DEPENDS}
		)
	endif()
endfunction()

# rush_shader_glsl(<shader> [DEPENDS <dep> ...])
function(rush_shader_glsl shaderName)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "DEPENDS")
	if(APPLE AND DEFINED RUSH_RENDER_API AND RUSH_RENDER_API STREQUAL "MTL")
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CFG_INTDIR}/${shaderName}.metallib
			BYPRODUCTS ${CMAKE_CFG_INTDIR}/${shaderName}.metal ${CMAKE_CFG_INTDIR}/${shaderName}.air
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}
			COMMAND ${RUSH_GLSLC} -O -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			COMMAND ${RUSH_SPIRV_CROSS} --msl --msl-version 230000 --msl-argument-buffers --msl-decoration-binding --msl-force-active-argument-buffer-resources ${CMAKE_CFG_INTDIR}/${shaderName}.bin > ${CMAKE_CFG_INTDIR}/${shaderName}.metal
			COMMAND ${RUSH_XCRUN} -sdk ${RUSH_METAL_SDK} metal -std=metal3.0 -c ${CMAKE_CFG_INTDIR}/${shaderName}.metal -o ${CMAKE_CFG_INTDIR}/${shaderName}.air
			COMMAND ${RUSH_XCRUN} -sdk ${RUSH_METAL_SDK} metallib ${CMAKE_CFG_INTDIR}/${shaderName}.air -o ${CMAKE_CFG_INTDIR}/${shaderName}.metallib
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			DEPENDS ${ARG_DEPENDS}
		)
	else()
		add_custom_command(
			OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin
			COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}
			COMMAND ${RUSH_GLSLC} -O -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
			DEPENDS ${ARG_DEPENDS}
		)
	endif()
endfunction()

# rush_shader_metal(<shader>)
function(rush_shader_metal shaderName)
	add_custom_command(
		OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.metallib
		BYPRODUCTS ${CMAKE_CFG_INTDIR}/${shaderName}.air
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}
		COMMAND ${RUSH_XCRUN} -sdk ${RUSH_METAL_SDK} metal -std=metal3.0 -c ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName} -o ${CMAKE_CFG_INTDIR}/${shaderName}.air
		COMMAND ${RUSH_XCRUN} -sdk ${RUSH_METAL_SDK} metallib ${CMAKE_CFG_INTDIR}/${shaderName}.air -o ${CMAKE_CFG_INTDIR}/${shaderName}.metallib
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
	)
endfunction()

# rush_shader_rt(<shader> [DEPENDS <dep> ...])
function(rush_shader_rt shaderName)
	cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "DEPENDS")
	add_custom_command(
		OUTPUT ${CMAKE_CFG_INTDIR}/${shaderName}.bin
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CFG_INTDIR}
		COMMAND ${RUSH_GLSLC} --target-env=vulkan1.2 -o ${CMAKE_CFG_INTDIR}/${shaderName}.bin ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${shaderName}
		DEPENDS ${ARG_DEPENDS}
	)
endfunction()
