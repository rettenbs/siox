#!/usr/bin/env python
# -*- coding: utf-8 -*-
# encoding: utf-8
from __future__ import print_function

import re
import sys
import argparse
import subprocess
# import the template
from template import *

DEBUG = False


##
# @brief Generate and handle the command line parsing.
#
# This class is just a simple Wrapper of the python argparser.
class  Option():

    def parse(self):
        prog='SIOX-Wrapper'

        description='''The SIOX-Wrapper is a tool which instrument software
libraries, by calling trace functions before and after the actual library call.'''

        argParser = argparse.ArgumentParser(description=description, prog=prog)

        argParser.add_argument('--output', '-o', action='store', nargs=1,
        dest='outputFile', default='out.h', help='Provide a file to write the output.')

        argParser.add_argument('--debug', '-d', action='store_true', default=False,
        dest='debug', help='''Print out debug information.''')

        argParser.add_argument('--blank-header', '-b', action='store_true',
                default=False, dest='blankHeader',
                help='''Generate a clean header file wich can be instrumented
from a other header file or C source file.''')

        argParser.add_argument('--cpp', '-c', action='store_true', default=False,
        dest='cpp', help='Use cpp to pre process the input file.')

        argParser.add_argument('--cpp-args', '-a', action='store',nargs=1, default=[],
        dest='cppArgs', help='''Pass arguments to the cpp. If this option is
specified the --cpp option is specified implied.''')

        argParser.add_argument('--alt-var-names', '-n',
        action='store_true', default=False, dest='alternativeVarNames',
        help='''If a header file dose not provide variable names, this will
enumerate them.''')

        argParser.add_argument('inputFile', default=None,
        help='Source or header file to parse')

        args = argParser.parse_args()

        if args.outputFile: args.outputFile = args.outputFile[0]
        if args.cppArgs: args.cpp = True

        return args

##
# @brief A storage class for a function.
#
#
class Function():

    def __init__(self):

        ## Return type of the function (int*, char, etc.)
        self.type= ''
        ## Name of the function
        self.name = ''
        ## A list of Parameters, a parameter is a extra class for storing.
        self.parameters = []
        ## A list of templates associated with the function.
        self.usedTemplates = []
        ## Indicates that the function is the first called function of the library and initialize SIOX.
        self.init = False
        ## Indicates that the function is the last called function of the library.
        self.final = False

    ##
    # @brief Generate the function call.
    #
    # @param self The reference to this object.
    #
    # @return A string containing the function call.
    #
    # Generates the function call of the function.
    def call(self):

        return '%s(%s)' % (self.name, ', '.join(paramName.name for paramName in self.parameters))


    ##
    # @brief Generate the function definition.
    #
    # @param self The reference to this object.
    #
    # @return The function definition as a string.
    def definition(self):

        return '%s %s(%s)'% (self.type, self.name,
                ', '.join('  '.join([param.type, param.name]) for param in self.parameters))
    ##
    # @brief Generate an identifier of the function.
    #
    # The generated identifier is used as a key for the hash table the functions
    # are stored in.
    #
    # @return
    def getIdentyfier(self):

        identifier = self.type
        identifier += self.name
        identifier +='('

        for parameter in self.parameters:
            identifier += ''.join(parameter.type.split())
            identifier += parameter.name
        identifier += ');'
        identifier = re.sub('[,\s]', '', identifier)
        return identifier
##
# @brief One parameter of a function.
class Parameter():

  def __init__(self):
    self.type = ''
    self.name = ''

  def __str__(self):
    return self.name
  #end def
#end class

class Instruction():

  def __init__(self):
    self.name = ''
    self.parameters = []
  #end def
#end class

##
# @brief This class parses the abstract syntax tree generated by the pycparser
class FunctionParser():

    ##
    # @brief
    #
    # @param options
    #
    # @return
    def __init__(self, options):

        self.inputFile = options.inputFile
        self.cpp = options.cpp
        self.cppArgs = options.cppArgs
        self.alternativeVarNames = options.alternativeVarNames
        self.functions = []
        # This regex searches from the beginning of the file or } or ; to the next
        # ; or {.
        self.regexFuncDef = re.compile('(?:;|})?(.+?)(?:;|{)',re.M | re.S)

        # Filter lines with comments beginng with / or # and key word that looks
        # like function definitions
        self.regexFilterList = [re.compile('^\s*[#/].*'),
                re.compile('^\s*typedef.*'),
                re.compile('^.*\].*$')
                ]

        # Split every line that is left in function name and parameters.
        self.regexFuncDefParts = re.compile( '(.+?)\((.+?)\).*')

    ##
    # @brief
    #
    # @return
    def parse(self):

        functions = []
        lines = ''

        if self.cpp:
            cppCommand = ['cpp']
            cppCommand.extend(self.cppArgs)
            cppCommand.append(self.inputFile)
            cpp = subprocess.Popen(cppCommand,
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            lines, error = cpp.communicate()

        else:
            #FIXME
            inputFile = open(self.inputFile, 'r')
            lines = inputFile.read()

        matchFuncDef = self.regexFuncDef.findall(lines)

        for funcNo, function in enumerate(matchFuncDef):


            function = function.split('\n')
            for index in range(0,len(function)):
                function[index] = function[index].lstrip()

                for regexFilter in self.regexFilterList:
                    if regexFilter.match(function[index]):
                        function[index] = ''

                function[index] = function[index].rstrip()

            function = ''.join(function)

            funcParts = self.regexFuncDefParts.match(function)

            if funcParts is None:
                continue

            function = Function()
            functions.append(function)
            function.type, function.name = self.getTypeName(funcParts.group(1),
            'function'+str(funcNo))

            parameters = funcParts.group(2).split(',')

            for paramNo, parameter in enumerate(parameters):
                parameterObj = Parameter()
                function.parameters.append(parameterObj)
                parameterObj.type, parameterObj.name = self.getTypeName(parameter, 'var'+str(paramNo))

            function.generateSignature()
        return functions

    ##
    # @brief
    #
    # @param declaration
    # @param alternativeVarName
    #
    # @return
    def getTypeName(self, declaration, alternativeVarName):

        type = ''
        name = ''

        parameterParts = declaration.split('*')


        if self.alternativeVarNames:
            if(len(parameterParts) == 1):
                type = parameterParts[0].strip()
                name = alternativeName.strip()

            else:
                for element in parameterParts:
                    element = element.strip()
                    if element is '':
                        type += '*'

                    else:
                        type += ' %s' % (element)
                        name = alternativeName
                        type += '*'

        else:

            if(len(parameterParts) == 1):
                parameterParts = declaration.split()
                name = parameterParts.pop().strip()

                for element in parameterParts:
                    element = element.strip()
                    type += ' %s' % (element)

            else:
                name = parameterParts.pop().strip()

                for element in parameterParts:
                    element = element.strip()
                    if element is '':
                        type+= '*'

                    else:
                        type+= '  %s' % (element)
                        type += '*'
        return (type.strip(), name)

##
# @brief
class CommandParser():

    def __init__(self,options):
        self.inputFile = options.inputFile

        self.commandRegex = re.compile('^\s*//\s*(.+?)\s+(.*)')

    def parse(self, functions):
        avalibalCommands  = template.keys()
        input = open(self.inputFile, 'r')
        inputLines = input.readlines()
        index = 0
        commandName=''
        commandArgs=''
        templateList = []
        for line in inputLines:
            match = self.commandRegex.match(line)
            if (match):
                if match.group(1) == 'init':
                    functions[index].init = True

                elif match.group(1) == 'final':
                    functions[index].final = True

                elif match.group(1) in avalibalCommands:
                    if commandName != '':
                        templateList.append(templateClass(commandName, commandArgs))
                        commandName = ''
                        commandArgs = ''

                    commandName += match.group(1)
                    commandArgs += match.group(2)

                else:
                    if commandName == '':
                        out("error", file=sys.stderr)
                        sys.exit(1)

                    commandArgs = match.group(1)
                    commandArgs = match.group(2)
            else:
                if re.sub('[,\s]', '', line) == functions[index].getIdentyfier():
                    if commandName != '':
                        templateList.append(templateClass(commandName, commandArgs))
                        functions[index].usedTemplates = templateList
                        templateList = []
                        commandName = ''
                        commandArgs = ''
                    index = index + 1

        return functions

##
# @brief Used to store the templates for each funtion
class templateClass():
    ##
    # @brief The constructor
    #
    # @param name The name of the used template
    # @param variables The used variables
    def __init__(self, name, variables):
        templateDict = template[name]
        self.name = name
        self.parameters = {}

        # Generate strings for output from given input
        self.setParameters(templateDict['variables'], variables)

        # Remember template-acces for easier usage
        self.world = templateDict['global']
        self.init = templateDict['init']
        self.before = templateDict['before']
        self.after = templateDict['after']
        self.final = templateDict['final']

    ##
    # @brief Reads the parameteres and generates the needed output
    #
    # @param names The name of the used template
    # @param values The used variables
    def setParameters(self, names, values):
        # generate a list of all names and values
        NameList = names.split(' ')
        ValueList = values.split(' ')

        position = 0;
        # iterate over all elements but the last one
        for value in ValueList[0:len(NameList)-1]:
            self.parameters[NameList[position]] = value
            position += 1
        # all other elements belong to the last parameter
        self.parameters[NameList[position]] = ' '.join(ValueList[len(NameList)-1:])

    ##
    # @brief Used for selective output
    #
    # @param type What part of the template should be given
    #
    # @return The requested string from the template
    def output(self, type):
        if (type == 'global'):
            return self.world % self.parameters
        elif (type == 'init'):
            return self.init % self.parameters
        elif (type == 'before'):
            return self.before % self.parameters
        elif (type == 'after'):
            return self.after % self.parameters
        elif (type == 'final'):
            return self.final % self.parameters
        else:
            # Error
            # FIXME: Proper Error-Handling
            return 'Einmal mit Profis arbeiten...'

##
# @brief The output class (write a file to disk)
#
class Writer():

    ##
    # @brief The constructor
    #
    # @param options The supplied arguments
    def __init__(self, options):
        self.outputFile = options.outputFile

    ##
    # @brief Write a header file
    #
    # @param functions A list of function-objects to write
    def headerFile(self, functions):
        # open the output file for writing
        file = open(self.outputFile, 'w')

        # write all function headers
        for function in functions:
            file.write("%s ; \n" % (function.type, function.call()))

        # close the file
        file.close()

    ##
    # @brief Write a source file
    #
    # @param functions A list of function-objects to write
    def sourceFile(self, functions):
        # open the output file for writing
        file = open(self.outputFile, 'w')

        # write all needed includes
        file.write('#include <stdio.h> \n#include <stdlib.h>\n\n')

        # write all global-Templates
        for temp in functions[0].usedTemplates:
            file.write('%s\n' % (temp.output('global')))
        file.write("\n\n")

        # write the redefinition of all functions
        for function in functions:
            file.write('%s *__real_%s;\n' % (function.type, function.call()))

        # write all functions-bodies
        for function in functions:
            # write function signature
            file.write('%s *__wrap_%s\n{\n' % (function.type, function.call()))

            # a variable to save the return-value
            file.write('\t%s ret;\n' % (function.type))

            # is this the desired init-function?
            if function.init:
                # write all init-templates
                for func in functions:
                    for temp in func.usedTemplates:
                        outstr = '\t%s\n' % (temp.output('init').strip())
                        if outstr.strip() != '':
                            file.write(outstr)

            # write the before-template for this function
            for temp in function.usedTemplates:
                outstr = '\t%s\n' % (temp.output('before').strip())
                if outstr.strip() != '':
                    file.write(outstr)

            # generate a string with all parameter-names
            signatureNames = ', '.join(paramter.name for parameter in function.parameters)

            # write the function call
            file.write('\tret = __real_%s(%s);\n' % (function.name, signatureNames))

            # is this the desired final-function?
            if function.final:
                # write all final-functions
                for func in functions:
                    for temp in func.usedTemplates:
                        outstr = '\t%s\n' % (temp.output('final').strip())
                        if outstr.strip() != '':
                            file.write(outstr)

            # write all after-templates for this function
            for temp in function.usedTemplates:
                outstr = '\t%s\n' % (temp.output('after').strip())
                if outstr.strip() != '':
                    file.write(outstr)

            # write the return statement and close the function
            file.write('\treturn ret;\n}\n\n')

    # close the file
    file.close

##
# @brief The main function.
#
def main():

  # Parse input parameter.
    opt = Option()
    options = opt.parse()


    functionParser = FunctionParser(options)
    outputWriter = Writer(options)

    functions = functionParser.parse()

    if options.blankHeader:
        outputWriter.headerFile(functions)

    else:
        commandParser = CommandParser(options)
        functions = commandParser.parse(functions)
        outputWriter.sourceFile(functions)

if __name__  == '__main__':
  main()


