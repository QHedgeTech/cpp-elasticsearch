import os
import platform
	
#get the mode flag from the command line
mode = ARGUMENTS.get('mode', 'release')   		#default to 'debug' if the user didn't specify
compiler = ARGUMENTS.get('compiler', 'gnu')   	#default to 'gnu' if the user didn't specify
project = ARGUMENTS.get('project', '')   		#holds current project to compiler

#check if the user has been naughty: only 'debug','profile' or 'release' allowed
if not (mode in ['debug', 'profile', 'release']):
	print "Error: expected 'debug', 'profile' or 'release', found: " + mode
	Exit(1)

#check if the user has been naughty: only 'debug','profile' or 'release' allowed
if not (compiler in ['gnu', 'clang']):
	print "Error: expected 'gnu' or 'clang', found: " + compiler
	Exit(1)
		
#tell the user what we're doing
print '**** Compiling with '+ compiler +' in ' + mode + ' mode'

env = Environment()

env['ENV']['PATH'] = os.environ['PATH']

mycflags = []
mylinkflags = []

#define environment and modify default compiler
if compiler == 'gnu':
	env['CC'] = 'gcc'
	env['CXX'] = 'g++'
	#-Wl,--no-as-needed
	if platform.system() == 'Darwin':
		env['CC'] = 'gcc-4.9'
		env['CXX'] = 'g++-4.9'
		env['CCVERSION'] = '4.9'
		env['CXXVERSION'] = '4.9'
	
	# Needed with gcc to compile the threads, don't ask me why...
	mylinkflags.append('-pthread')
	print 'We compile with GNU compiler'

if compiler == 'clang':
	env['CC'] = 'clang'
	env['CXX'] = 'clang++'
	print 'We compile with Clang compiler'


#extra compile flags		
if mode == 'debug':
   	mycflags = ['-W','-Wfatal-errors','-Wall','-Wextra', '-pedantic', '-g']

if mode == 'profile':
	mycflags = ['-W','-Wall','-Wfatal-errors','-Wextra','-pg','-DNDEBUG']
	mylinkflags = ['-pg']
	
if mode == 'release':
	mycflags = ['-W','-Wall','-Wfatal-errors','-O3', '-DNDEBUG']

#append the user's additional compile flags
env.Append(CCFLAGS= '-std=c++11')
env.Append(CXXFLAGS= mycflags)
env.Append(LINKFLAGS= mylinkflags)

#make sure the sconscripts can get to the variables
Export('mode','compiler','project')

#put all .sconsign files in one place
env.SConsignFile()

#specify the sconscript for the project
prog = SConscript('example/'+ project + '/SConscript', exports = 'env')

