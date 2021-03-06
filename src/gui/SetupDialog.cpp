/*
 * SetupDialog.cpp - dialog for setting up LMMS
 *
 * Copyright (c) 2005-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include <QComboBox>
#include <QImageReader>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollArea>

#include "SetupDialog.h"
#include "TabBar.h"
#include "TabButton.h"
#include "gui_templates.h"
#include "Mixer.h"
#include "MainWindow.h"
#include "ProjectJournal.h"
#include "embed.h"
#include "Engine.h"
#include "debug.h"
#include "ToolTip.h"
#include "FileDialog.h"


// platform-specific audio-interface-classes
#include "AudioAlsa.h"
#include "AudioAlsaSetupWidget.h"
#include "AudioJack.h"
#include "AudioOss.h"
#include "AudioSndio.h"
#include "AudioPortAudio.h"
#include "AudioSoundIo.h"
#include "AudioPulseAudio.h"
#include "AudioSdl.h"
#include "AudioDummy.h"

// platform-specific midi-interface-classes
#include "MidiAlsaRaw.h"
#include "MidiAlsaSeq.h"
#include "MidiJack.h"
#include "MidiOss.h"
#include "MidiSndio.h"
#include "MidiWinMM.h"
#include "MidiApple.h"
#include "MidiDummy.h"

constexpr int BUFFERSIZE_RESOLUTION = 32;

inline void labelWidget( QWidget * _w, const QString & _txt )
{
	QLabel * title = new QLabel( _txt, _w );
	QFont f = title->font();
	f.setBold( true );
	title->setFont( pointSize<12>( f ) );


	assert( dynamic_cast<QBoxLayout *>( _w->layout() ) != NULL );

	dynamic_cast<QBoxLayout *>( _w->layout() )->addSpacing( 5 );
	dynamic_cast<QBoxLayout *>( _w->layout() )->addWidget( title );
	dynamic_cast<QBoxLayout *>( _w->layout() )->addSpacing( 10 );
}




SetupDialog::SetupDialog( ConfigTabs _tab_to_open ) :
	m_bufferSize( ConfigManager::inst()->value( "mixer",
					"framesperaudiobuffer" ).toInt() ),
	m_toolTips( !ConfigManager::inst()->value( "tooltips",
							"disabled" ).toInt() ),
	m_warnAfterSetup( !ConfigManager::inst()->value( "app",
						"nomsgaftersetup" ).toInt() ),
	m_displaydBFS( ConfigManager::inst()->value( "app", 
		      				"displaydbfs" ).toInt() ),
	m_MMPZ( !ConfigManager::inst()->value( "app", "nommpz" ).toInt() ),
	m_disableBackup( !ConfigManager::inst()->value( "app",
							"disablebackup" ).toInt() ),
	m_openLastProject( ConfigManager::inst()->value( "app",
							"openlastproject" ).toInt() ),
	m_hqAudioDev( ConfigManager::inst()->value( "mixer",
							"hqaudio" ).toInt() ),
	m_lang( ConfigManager::inst()->value( "app",
							"language" ) ),
	m_workingDir( QDir::toNativeSeparators( ConfigManager::inst()->workingDir() ) ),
	m_vstDir( QDir::toNativeSeparators( ConfigManager::inst()->vstDir() ) ),
	m_artworkDir( QDir::toNativeSeparators( ConfigManager::inst()->artworkDir() ) ),
	m_ladDir( QDir::toNativeSeparators( ConfigManager::inst()->ladspaDir() ) ),
	m_gigDir( QDir::toNativeSeparators( ConfigManager::inst()->gigDir() ) ),
	m_sf2Dir( QDir::toNativeSeparators( ConfigManager::inst()->sf2Dir() ) ),
#ifdef LMMS_HAVE_FLUIDSYNTH
	m_defaultSoundfont( QDir::toNativeSeparators( ConfigManager::inst()->defaultSoundfont() ) ),
#endif
#ifdef LMMS_HAVE_STK
	m_stkDir( QDir::toNativeSeparators( ConfigManager::inst()->stkDir() ) ),
#endif
	m_backgroundArtwork( QDir::toNativeSeparators( ConfigManager::inst()->backgroundArtwork() ) ),
	m_smoothScroll( ConfigManager::inst()->value( "ui", "smoothscroll" ).toInt() ),
	m_enableAutoSave( ConfigManager::inst()->value( "ui", "enableautosave", "1" ).toInt() ),
	m_enableRunningAutoSave( ConfigManager::inst()->value( "ui", "enablerunningautosave", "0" ).toInt() ),
	m_saveInterval(	ConfigManager::inst()->value( "ui", "saveinterval" ).toInt() < 1 ?
					MainWindow::DEFAULT_SAVE_INTERVAL_MINUTES :
			ConfigManager::inst()->value( "ui", "saveinterval" ).toInt() ),
	m_oneInstrumentTrackWindow( ConfigManager::inst()->value( "ui",
					"oneinstrumenttrackwindow" ).toInt() ),
	m_compactTrackButtons( ConfigManager::inst()->value( "ui",
					"compacttrackbuttons" ).toInt() ),
	m_syncVSTPlugins( ConfigManager::inst()->value( "ui",
							"syncvstplugins" ).toInt() ),
	m_animateAFP(ConfigManager::inst()->value( "ui",
						   "animateafp", "1" ).toInt() ),
	m_printNoteLabels(ConfigManager::inst()->value( "ui",
						   "printnotelabels").toInt() ),
	m_displayWaveform(ConfigManager::inst()->value( "ui",
						   "displaywaveform").toInt() ),
	m_disableAutoQuit(ConfigManager::inst()->value( "ui",
						   "disableautoquit").toInt() ),
	m_vstEmbedMethod( ConfigManager::inst()->vstEmbedMethod() )
{
	setWindowIcon( embed::getIconPixmap( "setup_general" ) );
	setWindowTitle( tr( "Setup LMMS" ) );
	setModal( true );
	setFixedSize( 452, 570 );

	Engine::projectJournal()->setJournalling( false );

	QVBoxLayout * vlayout = new QVBoxLayout( this );
	vlayout->setSpacing( 0 );
	vlayout->setMargin( 0 );
	QWidget * settings = new QWidget( this );
	QHBoxLayout * hlayout = new QHBoxLayout( settings );
	hlayout->setSpacing( 0 );
	hlayout->setMargin( 0 );

	m_tabBar = new TabBar( settings, QBoxLayout::TopToBottom );
	m_tabBar->setExclusive( true );
	m_tabBar->setFixedWidth( 72 );

	QWidget * ws = new QWidget( settings );
	int wsHeight = 420;
#ifdef LMMS_HAVE_STK
	wsHeight += 50;
#endif
#ifdef LMMS_HAVE_FLUIDSYNTH
	wsHeight += 50;
#endif
	ws->setFixedSize( 360, wsHeight );
	QWidget * general = new QWidget( ws );
	general->setFixedSize( 360, 290 );
	QVBoxLayout * gen_layout = new QVBoxLayout( general );
	gen_layout->setSpacing( 0 );
	gen_layout->setMargin( 0 );
	labelWidget( general, tr( "General settings" ) );

	TabWidget * bufsize_tw = new TabWidget( tr( "BUFFER SIZE" ), general );
	bufsize_tw->setFixedHeight( 80 );

	m_bufSizeSlider = new QSlider( Qt::Horizontal, bufsize_tw );
	m_bufSizeSlider->setRange( 1, 128 );
	m_bufSizeSlider->setTickPosition( QSlider::TicksBelow );
	m_bufSizeSlider->setPageStep( 8 );
	m_bufSizeSlider->setTickInterval( 8 );
	m_bufSizeSlider->setGeometry( 10, 16, 340, 18 );
	m_bufSizeSlider->setValue( m_bufferSize / BUFFERSIZE_RESOLUTION );

	connect( m_bufSizeSlider, SIGNAL( valueChanged( int ) ), this,
						SLOT( setBufferSize( int ) ) );

	m_bufSizeLbl = new QLabel( bufsize_tw );
	m_bufSizeLbl->setGeometry( 10, 40, 200, 32 );
	setBufferSize( m_bufSizeSlider->value() );

	QPushButton * bufsize_reset_btn = new QPushButton(
			embed::getIconPixmap( "reload" ), "", bufsize_tw );
	bufsize_reset_btn->setGeometry( 320, 40, 28, 28 );
	connect( bufsize_reset_btn, SIGNAL( clicked() ), this,
						SLOT( resetBufSize() ) );
	ToolTip::add( bufsize_reset_btn, tr( "Reset to default value" ) );

	TabWidget * misc_tw = new TabWidget( tr( "MISC" ), general );
	const int XDelta = 10;
	const int YDelta = 18;
	const int HeaderSize = 30;
	int labelNumber = 0;

	auto addLedCheckBox = [&XDelta, &YDelta, &misc_tw, &labelNumber, this](
		const char* ledText,
		bool initialState,
		const char* toggledSlot
	){
		LedCheckBox * checkBox = new LedCheckBox(tr(ledText), misc_tw);
		labelNumber++;
		checkBox->move(XDelta, YDelta*labelNumber);
		checkBox->setChecked(initialState);
		connect(checkBox, SIGNAL(toggled(bool)), this, toggledSlot);
	};

	addLedCheckBox("Enable tooltips",
		m_toolTips, SLOT(toggleToolTips(bool)));
	addLedCheckBox("Show restart warning after changing settings",
		m_warnAfterSetup, SLOT(toggleWarnAfterSetup(bool)));
	addLedCheckBox("Display volume as dBFS ",
		m_displaydBFS, SLOT(toggleDisplaydBFS(bool)));
	addLedCheckBox("Compress project files per default",
		m_MMPZ, SLOT(toggleMMPZ(bool)));
	addLedCheckBox("One instrument track window mode",
		m_oneInstrumentTrackWindow,
		SLOT(toggleOneInstrumentTrackWindow(bool)));
	addLedCheckBox("HQ-mode for output audio-device",
		m_hqAudioDev, SLOT(toggleHQAudioDev(bool)));
	addLedCheckBox("Compact track buttons",
		m_compactTrackButtons, SLOT(toggleCompactTrackButtons(bool)));
	addLedCheckBox("Sync VST plugins to host playback",
		m_syncVSTPlugins, SLOT(toggleSyncVSTPlugins(bool)));
	addLedCheckBox("Enable note labels in piano roll",
		m_printNoteLabels, SLOT(toggleNoteLabels(bool)));
	addLedCheckBox("Enable waveform display by default",
		m_displayWaveform, SLOT(toggleDisplayWaveform(bool)));
	addLedCheckBox("Keep effects running even without input",
		m_disableAutoQuit, SLOT(toggleDisableAutoquit(bool)));
	addLedCheckBox("Create backup file when saving a project",
		m_disableBackup, SLOT(toggleDisableBackup(bool)));
	addLedCheckBox("Reopen last project on start",
		m_openLastProject, SLOT(toggleOpenLastProject(bool)));

	misc_tw->setFixedHeight( YDelta*labelNumber + HeaderSize );

	TabWidget* embed_tw = new TabWidget( tr( "PLUGIN EMBEDDING" ), general);
	embed_tw->setFixedHeight( 48 );
	m_vstEmbedComboBox = new QComboBox( embed_tw );
	m_vstEmbedComboBox->move( XDelta, YDelta );

	QStringList embedMethods = ConfigManager::availabeVstEmbedMethods();
	m_vstEmbedComboBox->addItem( tr( "No embedding" ), "none" );
	if( embedMethods.contains("qt") )
	{
		m_vstEmbedComboBox->addItem( tr( "Embed using Qt API" ), "qt" );
	}
	if( embedMethods.contains("win32") )
	{
		m_vstEmbedComboBox->addItem( tr( "Embed using native Win32 API" ), "win32" );
	}
	if( embedMethods.contains("xembed") )
	{
		m_vstEmbedComboBox->addItem( tr( "Embed using XEmbed protocol" ), "xembed" );
	}
	m_vstEmbedComboBox->setCurrentIndex( m_vstEmbedComboBox->findData( m_vstEmbedMethod ) );

	TabWidget * lang_tw = new TabWidget( tr( "LANGUAGE" ), general );
	lang_tw->setFixedHeight( 48 );
	QComboBox * changeLang = new QComboBox( lang_tw );
	changeLang->move( XDelta, YDelta );

	QDir dir( ConfigManager::inst()->localeDir() );
	QStringList fileNames = dir.entryList( QStringList( "*.qm" ) );
	for( int i = 0; i < fileNames.size(); ++i )
	{
		// get locale extracted by filename
		fileNames[i].truncate( fileNames[i].lastIndexOf( '.' ) );
		m_languages.append( fileNames[i] );
		QString lang = QLocale( m_languages.last() ).nativeLanguageName();
		changeLang->addItem( lang );
	}
	connect( changeLang, SIGNAL( currentIndexChanged( int ) ),
							this, SLOT( setLanguage( int ) ) );

	//If language unset, fallback to system language when available
	if( m_lang == "" )
	{
		QString tmp = QLocale::system().name().left( 2 );
		if( m_languages.contains( tmp ) )
		{
			m_lang = tmp;
		}
		else
		{
			m_lang = "en";
		}
	}

	for( int i = 0; i < changeLang->count(); ++i )
	{
		if( m_lang == m_languages.at( i ) )
		{
			changeLang->setCurrentIndex( i );
			break;
		}
	}

	gen_layout->addWidget( bufsize_tw );
	gen_layout->addSpacing( 10 );
	gen_layout->addWidget( misc_tw );
	gen_layout->addSpacing( 10 );
	gen_layout->addWidget( embed_tw );
	gen_layout->addSpacing( 10 );
	gen_layout->addWidget( lang_tw );
	gen_layout->addStretch();



	QWidget * paths = new QWidget( ws );
	int pathsHeight = 420;
#ifdef LMMS_HAVE_STK
	pathsHeight += 55;
#endif
#ifdef LMMS_HAVE_FLUIDSYNTH
	pathsHeight += 55;
#endif
	paths->setFixedSize( 360, pathsHeight );
	QVBoxLayout * dir_layout = new QVBoxLayout( paths );
	dir_layout->setSpacing( 0 );
	dir_layout->setMargin( 0 );
	labelWidget( paths, tr( "Paths" ) );
	QLabel * title = new QLabel( tr( "Directories" ), paths );
	QFont f = title->font();
	f.setBold( true );
	title->setFont( pointSize<12>( f ) );


	QScrollArea *pathScroll = new QScrollArea( paths );

	QWidget *pathSelectors = new QWidget( ws );
	QVBoxLayout *pathSelectorLayout = new QVBoxLayout;
	pathScroll->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
	pathScroll->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	pathScroll->resize( 362, pathsHeight - 50  );
	pathScroll->move( 0, 30 );
	pathSelectors->resize( 360, pathsHeight - 50 );

	const int txtLength = 284;
	const int btnStart = 297;


	auto addPathEntry = [&](const char* caption,
		const QString& content,
		const char* setSlot,
		const char* openSlot,
		QLineEdit*& lineEdit,
		QWidget* twParent,
		const char* pixmap = "project_open")
	{
		TabWidget * newTw = new TabWidget(tr(caption).toUpper(),
					twParent);
		newTw->setFixedHeight(48);

		lineEdit = new QLineEdit(content, newTw);
		lineEdit->setGeometry(10, 20, txtLength, 16);
		connect(lineEdit, SIGNAL(textChanged(const QString &)),
			this, setSlot);

		QPushButton * selectBtn = new QPushButton(
			embed::getIconPixmap(pixmap, 16, 16),
			"", newTw);
		selectBtn->setFixedSize(24, 24);
		selectBtn->move(btnStart, 16);
		connect(selectBtn, SIGNAL(clicked()), this, openSlot);

		pathSelectorLayout->addWidget(newTw);
		pathSelectorLayout->addSpacing(10);
	};

	addPathEntry("LMMS working directory", m_workingDir,
		SLOT(setWorkingDir(const QString &)),
		SLOT(openWorkingDir()),
		m_wdLineEdit, pathSelectors);
	addPathEntry("GIG directory", m_gigDir,
		SLOT(setGIGDir(const QString &)),
		SLOT(openGIGDir()),
		m_gigLineEdit, pathSelectors);
	addPathEntry("SF2 directory", m_sf2Dir,
		SLOT(setSF2Dir(const QString &)),
		SLOT(openSF2Dir()),
		m_sf2LineEdit, pathSelectors);
	addPathEntry("VST-plugin directory", m_vstDir,
		SLOT(setVSTDir(const QString &)),
		SLOT(openVSTDir()),
		m_vdLineEdit, pathSelectors);
	addPathEntry("LADSPA plugin directories", m_ladDir,
		SLOT(setLADSPADir(const QString &)),
		SLOT(openLADSPADir()),
		m_ladLineEdit, paths,
		"add_folder");
#ifdef LMMS_HAVE_STK
	addPathEntry("STK rawwave directory", m_stkDir,
		SLOT(setSTKDir(const QString &)),
		SLOT(openSTKDir()),
		m_stkLineEdit, paths);
#endif
#ifdef LMMS_HAVE_FLUIDSYNTH
	addPathEntry("Default Soundfont File", m_defaultSoundfont,
		SLOT(setDefaultSoundfont(const QString &)),
		SLOT(openDefaultSoundfont()),
		m_sfLineEdit, paths);
#endif
	addPathEntry("Themes directory", m_artworkDir,
		SLOT(setArtworkDir(const QString &)),
		SLOT(openArtwortDir()),
		m_adLineEdit, pathSelectors);
	pathSelectorLayout->addStretch();
	addPathEntry("Background artwork", m_backgroundArtwork,
		SLOT(setBackgroundArtwork(const QString &)),
		SLOT(openBackgroundArtwork()),
		m_baLineEdit, paths);
	pathSelectors->setLayout(pathSelectorLayout);


	dir_layout->addWidget(pathSelectors);

	pathScroll->setWidget(pathSelectors);
	pathScroll->setWidgetResizable(true);



	QWidget * performance = new QWidget( ws );
	performance->setFixedSize( 360, 200 );
	QVBoxLayout * perf_layout = new QVBoxLayout( performance );
	perf_layout->setSpacing( 0 );
	perf_layout->setMargin( 0 );
	labelWidget( performance, tr( "Performance settings" ) );


	TabWidget * auto_save_tw = new TabWidget(
			tr( "Auto save" ).toUpper(), performance );
	auto_save_tw->setFixedHeight( 110 );

	m_saveIntervalSlider = new QSlider( Qt::Horizontal, auto_save_tw );
	m_saveIntervalSlider->setRange( 1, 20 );
	m_saveIntervalSlider->setTickPosition( QSlider::TicksBelow );
	m_saveIntervalSlider->setPageStep( 1 );
	m_saveIntervalSlider->setTickInterval( 1 );
	m_saveIntervalSlider->setGeometry( 10, 16, 340, 18 );
	m_saveIntervalSlider->setValue( m_saveInterval );

	connect( m_saveIntervalSlider, SIGNAL( valueChanged( int ) ), this,
						SLOT( setAutoSaveInterval( int ) ) );

	m_saveIntervalLbl = new QLabel( auto_save_tw );
	m_saveIntervalLbl->setGeometry( 10, 40, 200, 24 );
	setAutoSaveInterval( m_saveIntervalSlider->value() );

	m_autoSave = new LedCheckBox(
			tr( "Enable auto-save" ), auto_save_tw );
	m_autoSave->move( 10, 70 );
	m_autoSave->setChecked( m_enableAutoSave );
	connect( m_autoSave, SIGNAL( toggled( bool ) ),
				this, SLOT( toggleAutoSave( bool ) ) );

	m_runningAutoSave = new LedCheckBox(
			tr( "Allow auto-save while playing" ), auto_save_tw );
	m_runningAutoSave->move( 20, 90 );
	m_runningAutoSave->setChecked( m_enableRunningAutoSave );
	connect( m_runningAutoSave, SIGNAL( toggled( bool ) ),
				this, SLOT( toggleRunningAutoSave( bool ) ) );

	QPushButton * autoSaveResetBtn = new QPushButton(
			embed::getIconPixmap( "reload" ), "", auto_save_tw );
	autoSaveResetBtn->setGeometry( 320, 70, 28, 28 );
	connect( autoSaveResetBtn, SIGNAL( clicked() ), this,
						SLOT( resetAutoSave() ) );
	ToolTip::add( autoSaveResetBtn, tr( "Reset to default value" ) );

	m_saveIntervalSlider->setEnabled( m_enableAutoSave );
	m_runningAutoSave->setVisible( m_enableAutoSave );


	perf_layout->addWidget( auto_save_tw );
	perf_layout->addSpacing( 10 );


	TabWidget * ui_fx_tw = new TabWidget( tr( "UI effects vs. "
						"performance" ).toUpper(),
								performance );
	ui_fx_tw->setFixedHeight( 70 );

	LedCheckBox * smoothScroll = new LedCheckBox(
			tr( "Smooth scroll in Song Editor" ), ui_fx_tw );
	smoothScroll->move( 10, 20 );
	smoothScroll->setChecked( m_smoothScroll );
	connect( smoothScroll, SIGNAL( toggled( bool ) ),
				this, SLOT( toggleSmoothScroll( bool ) ) );

	LedCheckBox * animAFP = new LedCheckBox(
				tr( "Show playback cursor in AudioFileProcessor" ),
								ui_fx_tw );
	animAFP->move( 10, 40 );
	animAFP->setChecked( m_animateAFP );
	connect( animAFP, SIGNAL( toggled( bool ) ),
				this, SLOT( toggleAnimateAFP( bool ) ) );



	perf_layout->addWidget( ui_fx_tw );
	perf_layout->addStretch();



	QWidget * audio = new QWidget( ws );
	audio->setFixedSize( 360, 200 );
	QVBoxLayout * audio_layout = new QVBoxLayout( audio );
	audio_layout->setSpacing( 0 );
	audio_layout->setMargin( 0 );
	labelWidget( audio, tr( "Audio settings" ) );

	TabWidget * audioiface_tw = new TabWidget( tr( "AUDIO INTERFACE" ),
									audio );
	audioiface_tw->setFixedHeight( 60 );

	m_audioInterfaces = new QComboBox( audioiface_tw );
	m_audioInterfaces->setGeometry( 10, 20, 240, 22 );


	// create ifaces-settings-widget
	QWidget * asw = new QWidget( audio );
	asw->setFixedHeight( 60 );

	QHBoxLayout * asw_layout = new QHBoxLayout( asw );
	asw_layout->setSpacing( 0 );
	asw_layout->setMargin( 0 );
	//asw_layout->setAutoAdd( true );

#ifdef LMMS_HAVE_JACK
	m_audioIfaceSetupWidgets[AudioJack::name()] =
					new AudioJack::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_ALSA
	m_audioIfaceSetupWidgets[AudioAlsa::name()] =
					new AudioAlsaSetupWidget( asw );
#endif

#ifdef LMMS_HAVE_PULSEAUDIO
	m_audioIfaceSetupWidgets[AudioPulseAudio::name()] =
					new AudioPulseAudio::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_PORTAUDIO
	m_audioIfaceSetupWidgets[AudioPortAudio::name()] =
					new AudioPortAudio::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_SOUNDIO
	m_audioIfaceSetupWidgets[AudioSoundIo::name()] =
					new AudioSoundIo::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_SDL
	m_audioIfaceSetupWidgets[AudioSdl::name()] =
					new AudioSdl::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_OSS
	m_audioIfaceSetupWidgets[AudioOss::name()] =
					new AudioOss::setupWidget( asw );
#endif

#ifdef LMMS_HAVE_SNDIO
	m_audioIfaceSetupWidgets[AudioSndio::name()] =
					new AudioSndio::setupWidget( asw );
#endif
	m_audioIfaceSetupWidgets[AudioDummy::name()] =
					new AudioDummy::setupWidget( asw );


	for( AswMap::iterator it = m_audioIfaceSetupWidgets.begin();
				it != m_audioIfaceSetupWidgets.end(); ++it )
	{
		m_audioIfaceNames[tr( it.key().toLatin1())] = it.key();
	}
	for( trMap::iterator it = m_audioIfaceNames.begin();
				it != m_audioIfaceNames.end(); ++it )
	{
		QWidget * audioWidget = m_audioIfaceSetupWidgets[it.value()];
		audioWidget->hide();
		asw_layout->addWidget( audioWidget );
		m_audioInterfaces->addItem( it.key() );
	}

	// If no preferred audio device is saved, save the current one
	QString audioDevName = 
		ConfigManager::inst()->value( "mixer", "audiodev" );
	if( m_audioInterfaces->findText(audioDevName) < 0 )
	{
		audioDevName = Engine::mixer()->audioDevName();
		ConfigManager::inst()->setValue(
					"mixer", "audiodev", audioDevName );
	}
	m_audioInterfaces->
		setCurrentIndex( m_audioInterfaces->findText( audioDevName ) );
	m_audioIfaceSetupWidgets[audioDevName]->show();

	connect( m_audioInterfaces, SIGNAL( activated( const QString & ) ),
		this, SLOT( audioInterfaceChanged( const QString & ) ) );


	audio_layout->addWidget( audioiface_tw );
	audio_layout->addSpacing( 20 );
	audio_layout->addWidget( asw );
	audio_layout->addStretch();



	QWidget * midi = new QWidget( ws );
	QVBoxLayout * midi_layout = new QVBoxLayout( midi );
	midi_layout->setSpacing( 0 );
	midi_layout->setMargin( 0 );
	labelWidget( midi, tr( "MIDI settings" ) );

	TabWidget * midiiface_tw = new TabWidget( tr( "MIDI INTERFACE" ),
									midi );
	midiiface_tw->setFixedHeight( 60 );

	m_midiInterfaces = new QComboBox( midiiface_tw );
	m_midiInterfaces->setGeometry( 10, 20, 240, 22 );


	// create ifaces-settings-widget
	QWidget * msw = new QWidget( midi );
	msw->setFixedHeight( 60 );

	QHBoxLayout * msw_layout = new QHBoxLayout( msw );
	msw_layout->setSpacing( 0 );
	msw_layout->setMargin( 0 );
	//msw_layout->setAutoAdd( true );

#ifdef LMMS_HAVE_ALSA
	m_midiIfaceSetupWidgets[MidiAlsaSeq::name()] =
					MidiSetupWidget::create<MidiAlsaSeq>( msw );
	m_midiIfaceSetupWidgets[MidiAlsaRaw::name()] =
					MidiSetupWidget::create<MidiAlsaRaw>( msw );
#endif

#ifdef LMMS_HAVE_JACK
	m_midiIfaceSetupWidgets[MidiJack::name()] =
					MidiSetupWidget::create<MidiJack>( msw );
#endif

#ifdef LMMS_HAVE_OSS
	m_midiIfaceSetupWidgets[MidiOss::name()] =
					MidiSetupWidget::create<MidiOss>( msw );
#endif

#ifdef LMMS_HAVE_SNDIO
	m_midiIfaceSetupWidgets[MidiSndio::name()] =
					MidiSetupWidget::create<MidiSndio>( msw );
#endif

#ifdef LMMS_BUILD_WIN32
	m_midiIfaceSetupWidgets[MidiWinMM::name()] =
					MidiSetupWidget::create<MidiWinMM>( msw );
#endif

#ifdef LMMS_BUILD_APPLE
    m_midiIfaceSetupWidgets[MidiApple::name()] =
                    MidiSetupWidget::create<MidiApple>( msw );
#endif

	m_midiIfaceSetupWidgets[MidiDummy::name()] =
					MidiSetupWidget::create<MidiDummy>( msw );


	for( MswMap::iterator it = m_midiIfaceSetupWidgets.begin();
				it != m_midiIfaceSetupWidgets.end(); ++it )
	{
		m_midiIfaceNames[tr( it.key().toLatin1())] = it.key();
	}
	for( trMap::iterator it = m_midiIfaceNames.begin();
				it != m_midiIfaceNames.end(); ++it )
	{
		QWidget * midiWidget = m_midiIfaceSetupWidgets[it.value()];
		midiWidget->hide();
		msw_layout->addWidget( midiWidget );
		m_midiInterfaces->addItem( it.key() );
	}

	QString midiDevName = 
		ConfigManager::inst()->value( "mixer", "mididev" );
	if( m_midiInterfaces->findText(midiDevName) < 0 )
	{
		midiDevName = Engine::mixer()->midiClientName();
		ConfigManager::inst()->setValue(
					"mixer", "mididev", midiDevName );
	}
	m_midiInterfaces->setCurrentIndex( 
		m_midiInterfaces->findText( midiDevName ) );
	m_midiIfaceSetupWidgets[midiDevName]->show();

	connect( m_midiInterfaces, SIGNAL( activated( const QString & ) ),
		this, SLOT( midiInterfaceChanged( const QString & ) ) );


	midi_layout->addWidget( midiiface_tw );
	midi_layout->addSpacing( 20 );
	midi_layout->addWidget( msw );
	midi_layout->addStretch();


	m_tabBar->addTab( general, tr( "General settings" ), 0, false, true 
			)->setIcon( embed::getIconPixmap( "setup_general" ) );
	m_tabBar->addTab( paths, tr( "Paths" ), 1, false, true 
			)->setIcon( embed::getIconPixmap(
							"setup_directories" ) );
	m_tabBar->addTab( performance, tr( "Performance settings" ), 2, false,
				true )->setIcon( embed::getIconPixmap(
							"setup_performance" ) );
	m_tabBar->addTab( audio, tr( "Audio settings" ), 3, false, true
			)->setIcon( embed::getIconPixmap( "setup_audio" ) );
	m_tabBar->addTab( midi, tr( "MIDI settings" ), 4, true, true
			)->setIcon( embed::getIconPixmap( "setup_midi" ) );


	m_tabBar->setActiveTab( _tab_to_open );

	hlayout->addWidget( m_tabBar );
	hlayout->addSpacing( 10 );
	hlayout->addWidget( ws );
	hlayout->addSpacing( 10 );
	hlayout->addStretch();

	QWidget * buttons = new QWidget( this );
	QHBoxLayout * btn_layout = new QHBoxLayout( buttons );
	btn_layout->setSpacing( 0 );
	btn_layout->setMargin( 0 );
	QPushButton * ok_btn = new QPushButton( embed::getIconPixmap( "apply" ),
						tr( "OK" ), buttons );
	connect( ok_btn, SIGNAL( clicked() ), this, SLOT( accept() ) );

	QPushButton * cancel_btn = new QPushButton( embed::getIconPixmap(
								"cancel" ),
							tr( "Cancel" ),
							buttons );
	connect( cancel_btn, SIGNAL( clicked() ), this, SLOT( reject() ) );

	btn_layout->addStretch();
	btn_layout->addSpacing( 10 );
	btn_layout->addWidget( ok_btn );
	btn_layout->addSpacing( 10 );
	btn_layout->addWidget( cancel_btn );
	btn_layout->addSpacing( 10 );

	vlayout->addWidget( settings );
	vlayout->addSpacing( 10 );
	vlayout->addWidget( buttons );
	vlayout->addSpacing( 10 );
	vlayout->addStretch();

	show();


}




SetupDialog::~SetupDialog()
{
	Engine::projectJournal()->setJournalling( true );
}




void SetupDialog::accept()
{
	if( m_warnAfterSetup )
	{
		QMessageBox::information( NULL, tr( "Restart LMMS" ),
					tr( "Please note that most changes "
						"won't take effect until "
						"you restart LMMS!" ),
					QMessageBox::Ok );
	}

	// Hide dialog before setting values. This prevents an obscure bug
	// where non-embedded VST windows would steal focus and prevent LMMS
	// from taking mouse input, rendering the application unusable.
	QDialog::accept();

	ConfigManager::inst()->setValue( "mixer", "framesperaudiobuffer",
					QString::number( m_bufferSize ) );
	ConfigManager::inst()->setValue( "mixer", "audiodev",
			m_audioIfaceNames[m_audioInterfaces->currentText()] );
	ConfigManager::inst()->setValue( "mixer", "mididev",
			m_midiIfaceNames[m_midiInterfaces->currentText()] );
	ConfigManager::inst()->setValue( "tooltips", "disabled",
					QString::number( !m_toolTips ) );
	ConfigManager::inst()->setValue( "app", "nomsgaftersetup",
					QString::number( !m_warnAfterSetup ) );
	ConfigManager::inst()->setValue( "app", "displaydbfs",
					QString::number( m_displaydBFS ) );
	ConfigManager::inst()->setValue( "app", "nommpz",
						QString::number( !m_MMPZ ) );
	ConfigManager::inst()->setValue( "app", "disablebackup",
					QString::number( !m_disableBackup ) );
	ConfigManager::inst()->setValue( "app", "openlastproject",
					QString::number( m_openLastProject ) );
	ConfigManager::inst()->setValue( "mixer", "hqaudio",
					QString::number( m_hqAudioDev ) );
	ConfigManager::inst()->setValue( "ui", "smoothscroll",
					QString::number( m_smoothScroll ) );
	ConfigManager::inst()->setValue( "ui", "enableautosave",
					QString::number( m_enableAutoSave ) );
	ConfigManager::inst()->setValue( "ui", "saveinterval",
					QString::number( m_saveInterval ) );
	ConfigManager::inst()->setValue( "ui", "enablerunningautosave",
					QString::number( m_enableRunningAutoSave ) );
	ConfigManager::inst()->setValue( "ui", "oneinstrumenttrackwindow",
					QString::number( m_oneInstrumentTrackWindow ) );
	ConfigManager::inst()->setValue( "ui", "compacttrackbuttons",
					QString::number( m_compactTrackButtons ) );
	ConfigManager::inst()->setValue( "ui", "syncvstplugins",
					QString::number( m_syncVSTPlugins ) );
	ConfigManager::inst()->setValue( "ui", "animateafp",
					QString::number( m_animateAFP ) );
	ConfigManager::inst()->setValue( "ui", "printnotelabels",
					QString::number( m_printNoteLabels ) );
	ConfigManager::inst()->setValue( "ui", "displaywaveform",
					QString::number( m_displayWaveform ) );
	ConfigManager::inst()->setValue( "ui", "disableautoquit",
					QString::number( m_disableAutoQuit ) );
	ConfigManager::inst()->setValue( "app", "language", m_lang );
	ConfigManager::inst()->setValue( "ui", "vstembedmethod",
					m_vstEmbedComboBox->currentData().toString() );


	ConfigManager::inst()->setWorkingDir(QDir::fromNativeSeparators(m_workingDir));
	ConfigManager::inst()->setVSTDir(QDir::fromNativeSeparators(m_vstDir));
	ConfigManager::inst()->setGIGDir(QDir::fromNativeSeparators(m_gigDir));
	ConfigManager::inst()->setSF2Dir(QDir::fromNativeSeparators(m_sf2Dir));
	ConfigManager::inst()->setArtworkDir(QDir::fromNativeSeparators(m_artworkDir));
	ConfigManager::inst()->setLADSPADir(QDir::fromNativeSeparators(m_ladDir));
#ifdef LMMS_HAVE_FLUIDSYNTH
	ConfigManager::inst()->setDefaultSoundfont( m_defaultSoundfont );
#endif
#ifdef LMMS_HAVE_STK
	ConfigManager::inst()->setSTKDir(QDir::fromNativeSeparators(m_stkDir));
#endif	
	ConfigManager::inst()->setBackgroundArtwork( m_backgroundArtwork );

	// tell all audio-settings-widget to save their settings
	for( AswMap::iterator it = m_audioIfaceSetupWidgets.begin();
				it != m_audioIfaceSetupWidgets.end(); ++it )
	{
		it.value()->saveSettings();
	}
	// tell all MIDI-settings-widget to save their settings
	for( MswMap::iterator it = m_midiIfaceSetupWidgets.begin();
				it != m_midiIfaceSetupWidgets.end(); ++it )
	{
		it.value()->saveSettings();
	}

	ConfigManager::inst()->saveConfigFile();
}




void SetupDialog::setBufferSize( int _value )
{
	const int step = DEFAULT_BUFFER_SIZE / BUFFERSIZE_RESOLUTION;
	if( _value > step && _value % step )
	{
		int mod_value = _value % step;
		if( mod_value < step / 2 )
		{
			m_bufSizeSlider->setValue( _value - mod_value );
		}
		else
		{
			m_bufSizeSlider->setValue( _value + step - mod_value );
		}
		return;
	}

	if( m_bufSizeSlider->value() != _value )
	{
		m_bufSizeSlider->setValue( _value );
	}

	m_bufferSize = _value * BUFFERSIZE_RESOLUTION;
	m_bufSizeLbl->setText( tr( "Frames: %1\nLatency: %2 ms" ).arg(
					m_bufferSize ).arg(
						1000.0f * m_bufferSize /
				Engine::mixer()->processingSampleRate(),
						0, 'f', 1 ) );
}




void SetupDialog::resetBufSize()
{
	setBufferSize( DEFAULT_BUFFER_SIZE / BUFFERSIZE_RESOLUTION );
}




void SetupDialog::toggleToolTips( bool _enabled )
{
	m_toolTips = _enabled;
}




void SetupDialog::toggleWarnAfterSetup( bool _enabled )
{
	m_warnAfterSetup = _enabled;
}




void SetupDialog::toggleDisplaydBFS( bool _enabled )
{
	m_displaydBFS = _enabled;
}




void SetupDialog::toggleMMPZ( bool _enabled )
{
	m_MMPZ = _enabled;
}




void SetupDialog::toggleDisableBackup( bool _enabled )
{
	m_disableBackup = _enabled;
}




void SetupDialog::toggleOpenLastProject( bool _enabled )
{
	m_openLastProject = _enabled;
}




void SetupDialog::toggleHQAudioDev( bool _enabled )
{
	m_hqAudioDev = _enabled;
}




void SetupDialog::toggleSmoothScroll( bool _enabled )
{
	m_smoothScroll = _enabled;
}




void SetupDialog::toggleAutoSave( bool _enabled )
{
	m_enableAutoSave = _enabled;
	m_saveIntervalSlider->setEnabled( _enabled );
	m_runningAutoSave->setVisible( _enabled );
	setAutoSaveInterval( m_saveIntervalSlider->value() );
}




void SetupDialog::toggleRunningAutoSave( bool _enabled )
{
	m_enableRunningAutoSave = _enabled;
}




void SetupDialog::toggleCompactTrackButtons( bool _enabled )
{
	m_compactTrackButtons = _enabled;
}





void SetupDialog::toggleSyncVSTPlugins( bool _enabled )
{
	m_syncVSTPlugins = _enabled;
}

void SetupDialog::toggleAnimateAFP( bool _enabled )
{
	m_animateAFP = _enabled;
}


void SetupDialog::toggleNoteLabels( bool en )
{
	m_printNoteLabels = en;
}


void SetupDialog::toggleDisplayWaveform( bool en )
{
	m_displayWaveform = en;
}


void SetupDialog::toggleDisableAutoquit( bool en )
{
	m_disableAutoQuit = en;
}


void SetupDialog::toggleOneInstrumentTrackWindow( bool _enabled )
{
	m_oneInstrumentTrackWindow = _enabled;
}

void SetupDialog::setLanguage( int lang )
{
	m_lang = m_languages[lang];
}





void SetupDialog::openWorkingDir()
{
	QString new_dir = FileDialog::getExistingDirectory( this,
					tr( "Choose LMMS working directory" ), m_workingDir );
	if( ! new_dir.isEmpty() )
	{
		m_wdLineEdit->setText( new_dir );
	}
}

void SetupDialog::openGIGDir()
{
	QString new_dir = FileDialog::getExistingDirectory( this,
				tr( "Choose your GIG directory" ),
							m_gigDir );
	if( ! new_dir.isEmpty() )
	{
		m_gigLineEdit->setText( new_dir );
	}
}

void SetupDialog::openSF2Dir()
{
	QString new_dir = FileDialog::getExistingDirectory( this,
				tr( "Choose your SF2 directory" ),
							m_sf2Dir );
	if( ! new_dir.isEmpty() )
	{
		m_sf2LineEdit->setText( new_dir );
	}
}




void SetupDialog::setWorkingDir( const QString & _wd )
{
	m_workingDir = _wd;
}




void SetupDialog::openVSTDir()
{
	QString new_dir = FileDialog::getExistingDirectory( this,
				tr( "Choose your VST-plugin directory" ),
							m_vstDir );
	if( ! new_dir.isEmpty() )
	{
		m_vdLineEdit->setText( new_dir );
	}
}




void SetupDialog::setVSTDir( const QString & _vd )
{
	m_vstDir = _vd;
}

void SetupDialog::setGIGDir(const QString &_gd)
{
	m_gigDir = _gd;
}

void SetupDialog::setSF2Dir(const QString &_sfd)
{
	m_sf2Dir = _sfd;
}




void SetupDialog::openArtworkDir()
{
	QString new_dir = FileDialog::getExistingDirectory( this,
				tr( "Choose artwork-theme directory" ),
							m_artworkDir );
	if( ! new_dir.isEmpty() )
	{
		m_adLineEdit->setText( new_dir );
	}
}




void SetupDialog::setArtworkDir( const QString & _ad )
{
	m_artworkDir = _ad;
}




void SetupDialog::openLADSPADir()
{
	QString new_dir = FileDialog::getExistingDirectory( this,
				tr( "Choose LADSPA plugin directory" ),
							m_ladDir );
	if( ! new_dir.isEmpty() )
	{
		if( m_ladLineEdit->text() == "" )
		{
			m_ladLineEdit->setText( new_dir );
		}
		else
		{
			m_ladLineEdit->setText( m_ladLineEdit->text() + "," +
								new_dir );
		}
	}
}



void SetupDialog::openSTKDir()
{
#ifdef LMMS_HAVE_STK
	QString new_dir = FileDialog::getExistingDirectory( this,
				tr( "Choose STK rawwave directory" ),
							m_stkDir );
	if( ! new_dir.isEmpty() )
	{
		m_stkLineEdit->setText( new_dir );
	}
#endif
}




void SetupDialog::openDefaultSoundfont()
{
#ifdef LMMS_HAVE_FLUIDSYNTH
	QString new_file = FileDialog::getOpenFileName( this,
				tr( "Choose default SoundFont" ), m_defaultSoundfont, 
				"SoundFont2 Files (*.sf2)" );
	
	if( ! new_file.isEmpty() )
	{
		m_sfLineEdit->setText( new_file );
	}
#endif
}




void SetupDialog::openBackgroundArtwork()
{
	QList<QByteArray> fileTypesList = QImageReader::supportedImageFormats();
	QString fileTypes;
	for( int i = 0; i < fileTypesList.count(); i++ )
	{
		if( fileTypesList[i] != fileTypesList[i].toUpper() )
		{
			if( !fileTypes.isEmpty() )
			{
				fileTypes += " ";
			}
			fileTypes += "*." + QString( fileTypesList[i] );
		}
	}

	QString dir = ( m_backgroundArtwork.isEmpty() ) ?
		m_artworkDir :
		m_backgroundArtwork;
	QString new_file = FileDialog::getOpenFileName( this,
			tr( "Choose background artwork" ), dir, 
			"Image Files (" + fileTypes + ")" );
	
	if( ! new_file.isEmpty() )
	{
		m_baLineEdit->setText( new_file );
	}
}




void SetupDialog::setLADSPADir( const QString & _fd )
{
	m_ladDir = _fd;
}




void SetupDialog::setSTKDir( const QString & _fd )
{
#ifdef LMMS_HAVE_STK
	m_stkDir = _fd;
#endif
}




void SetupDialog::setDefaultSoundfont( const QString & _sf )
{
#ifdef LMMS_HAVE_FLUIDSYNTH
	m_defaultSoundfont = _sf;
#endif
}




void SetupDialog::setBackgroundArtwork( const QString & _ba )
{
	m_backgroundArtwork = _ba;
}




void SetupDialog::setAutoSaveInterval( int value )
{
	m_saveInterval = value;
	m_saveIntervalSlider->setValue( m_saveInterval );
	QString minutes = m_saveInterval > 1 ? tr( "minutes" ) : tr( "minute" );
	minutes = QString( "%1 %2" ).arg( QString::number( m_saveInterval ), minutes );
	minutes = m_enableAutoSave ?  minutes : tr( "Disabled" );
	m_saveIntervalLbl->setText( tr( "Auto-save interval: %1" ).arg( minutes ) );
}




void SetupDialog::resetAutoSave()
{
	setAutoSaveInterval( MainWindow::DEFAULT_SAVE_INTERVAL_MINUTES );
	m_autoSave->setChecked( true );
	m_runningAutoSave->setChecked( false );
}




void SetupDialog::audioInterfaceChanged( const QString & _iface )
{
	for( AswMap::iterator it = m_audioIfaceSetupWidgets.begin();
				it != m_audioIfaceSetupWidgets.end(); ++it )
	{
		it.value()->hide();
	}

	m_audioIfaceSetupWidgets[m_audioIfaceNames[_iface]]->show();
}




void SetupDialog::midiInterfaceChanged( const QString & _iface )
{
	for( MswMap::iterator it = m_midiIfaceSetupWidgets.begin();
				it != m_midiIfaceSetupWidgets.end(); ++it )
	{
		it.value()->hide();
	}

	m_midiIfaceSetupWidgets[m_midiIfaceNames[_iface]]->show();
}
