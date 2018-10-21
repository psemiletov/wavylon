project('wavylon', ['cpp','c'],
         default_options : ['cpp_std=c++11'],
         version : '5.3.5',
         license : 'GPLv3')

add_global_arguments('-DVERSION_NUMBER="5.3.5"', language : 'cpp')
add_global_arguments('-DUSE_QML_STUFF=1', language : 'cpp')

#harcoded for now
moc_params = ['-DQT_VERSION=0x050000', '-DUSE_QML_STUFF=1']

if build_machine.system() == 'linux'
moc_params += ['-DQ_OS_LINUX=1', '-DQ_OS_UNIX=1']
endif

compiler = meson.get_compiler('cpp')

sndfile_dep = compiler.find_library('sndfile', required : true)
samplerate_dep = compiler.find_library('samplerate', required : true)
portaudio_dep = compiler.find_library('portaudio', required : true)

qt5_dep = dependency('qt5', modules : ['Core', 'Gui', 'Widgets', 'Network', 'Qml', 'Quick']) 
qt5 = import('qt5') 


wavylon_headers_moc = ['afx.h',
'db.h',
'envelope.h',
'floatbuffer.h',
'fman.h',
'fx-filter.h',
'fx-panners.h',
'fxlist.h',
'fxpresets.h',
'fxrack.h',
'fxset.h',
'gui_utils.h',
'levelmeter.h',
'libretta_interpolator.h',
'logmemo.h',
'project.h',
'shortcuts.h',
#'tinyplayer.h',
'tio.h',
'utils.h',
'wavylon.h'
]
  

src_processed = qt5.preprocess(
                moc_headers : wavylon_headers_moc,
                moc_extra_arguments: moc_params,
                qresources  : 'wavylon.qrc')


wavylon_source = ['afx.cpp',
'db.cpp',
'envelope.cpp',
'floatbuffer.cpp',
'fman.cpp',
'fx-filter.cpp',
'fx-panners.cpp',
'fxlist.cpp',
'fxpresets.cpp',
'fxrack.cpp',
'fxset.cpp',
'gui_utils.cpp',
'levelmeter.cpp',
'libretta_interpolator.cpp',
'logmemo.cpp',
'main.cpp',
'project.cpp',
'shortcuts.cpp',
#'tinyplayer.cpp',
'tio.cpp',
'utils.cpp',
'wavylon.cpp'
]


wavylon_exe = executable ('wavylon',
  sources : [src_processed, wavylon_source], 
  install : true,
  dependencies : [qt5_dep, sndfile_dep, portaudio_dep, samplerate_dep]
  )


install_data(['icons/wavylon.png'],
             install_dir : 'share/icons/hicolor/64x64/apps')

install_data(['desktop/wavylon.desktop'],
             install_dir : 'share/applications')
