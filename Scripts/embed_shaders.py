import codecs
from collections import namedtuple
import os
import subprocess
import sys
import textwrap

BinText = namedtuple('BinText', 'declaration, definition')

def bin2cpp(filename, variableName):
	s = open(filename, 'rb').read()
	s = codecs.encode(s, "hex").decode().upper()

	t=",".join(["0x"+x+y for (x,y) in zip(s[0::2], s[1::2])])

	declarationText  = "extern const unsigned char " + variableName + "data[];"
	declarationText += "\n";
	declarationText += "extern const size_t " + variableName + "size;"

	definitionText  = "const unsigned char " + variableName + "data[] = {\n\t%s\n};"%" \n\t".join(textwrap.wrap(t,80))
	definitionText += "\n";
	definitionText += "const size_t " + variableName + "size = sizeof(" + variableName + "data);"

	return BinText(
		declaration = declarationText, 
		definition = definitionText
	)

# Compile shaders and generate .h / .cpp code

header = '#pragma once\n// clang-format off\nnamespace Rush\n{\n'
source = '#include "GfxEmbeddedShaders.h"\n// clang-format off\nnamespace Rush\n{\n'

Shader = namedtuple('Shader', 'filename, entry, target')

hlslShaders = [
	Shader(
		filename = "..\\Shaders\\Primitive.hlsl",
		entry = "psMain",
		target = "ps_5_0",
	),
	Shader(
		filename = "..\\Shaders\\Primitive.hlsl",
		entry = "psMainTextured",
		target = "ps_5_0",
	),
	Shader(
		filename = "..\\Shaders\\Primitive.hlsl",
		entry = "vsMain3D",
		target = "vs_5_0",
	),
	Shader(
		filename = "..\\Shaders\\Primitive.hlsl",
		entry = "vsMain2D",
		target = "vs_5_0",
	),
]

# SPIR-V shaders

for shader in hlslShaders:
	# TODO: find glslc.exe and report error if not found
	hlslCompiler = "glslc.exe"
	inputFilename = os.path.normpath(shader.filename)
	shaderDirectory = os.path.dirname(shader.filename)
	outputFilename = os.path.join(shaderDirectory, shader.entry+".spv")
	stageMap = dict(
		vs_5_0="vertex",
		ps_5_0="fragment"
	)
	command = [
		hlslCompiler,
		"-x", "hlsl",
		"-o", outputFilename,
		"-fentry-point=" + shader.entry,
		"-fshader-stage=" + stageMap[shader.target],
		#"-DVULKAN=1",
		inputFilename
	]
	print(" ".join(command))
	compileResult = subprocess.call(command, stdout=subprocess.DEVNULL)
	if compileResult != 0:
		exit(1)
	variableNamePrefix = "SPV_" + shader.entry + "_"
	text = bin2cpp(outputFilename, variableNamePrefix)
	header += text.declaration + "\n";
	source += text.definition + "\n";
	os.remove(outputFilename)

# DXBC shaders

for shader in hlslShaders:
	# TODO: find fxc.exe and report error if not found
	hlslCompiler = "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\fxc.exe"
	inputFilename = os.path.normpath(shader.filename)
	shaderDirectory = os.path.dirname(shader.filename)
	outputFilename = os.path.join(shaderDirectory, shader.entry+".dxbc")
	command = [
		hlslCompiler,
		"/nologo",
		"/Fo", outputFilename,
		"/E", shader.entry,
		"/T", shader.target,
		inputFilename
	]
	print(" ".join(command))
	compileResult = subprocess.call(command, stdout=subprocess.DEVNULL)
	if compileResult != 0:
		exit(1)
	variableNamePrefix = "DXBC_" + shader.entry + "_"
	text = bin2cpp(outputFilename, variableNamePrefix)
	header += text.declaration + "\n";
	source += text.definition + "\n";
	os.remove(outputFilename)

# Output results

header += "}"
source += "}"

with open("..\\Rush\\GfxEmbeddedShaders.h", "w") as fileOut:
	fileOut.write(header)

with open("..\\Rush\\GfxEmbeddedShaders.cpp", "w") as fileOut:
	fileOut.write(source)
