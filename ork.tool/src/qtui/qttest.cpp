////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <orktool/qtui/qtapp.h>

///////////////////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtmainwin.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/input/input.h>
#include <ork/lev2/gfx/dbgfontman.h>

#include <QtGui/QStyle>

// This include is relative to src/miniork which is temporarily added an a include search path.
// We'll need to come up with a long-term solution eventually.
//#include <test/platform_lev1/test_application.h>
//#include <application/ds/ds_application.h>

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/qtui/qtui.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/editor/edmainwin.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/thread.h>

//#include <dispatch/dispatch.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {


static const std::string ReadMangledName(const char *&input, orkvector<std::string> &names, std::string &subresult) {
    const char *q = input;
    
    if(*input == 'S') {
        int nameref;
        input++;
        if(*input == '_') {
            nameref = 0;
        } else if(isdigit(*input)) {
            nameref = 1 + strtol(input, const_cast<char **>(&input), 10);
            OrkAssert(*input == '_');
        } else switch(*input++) {
        case 'a': return "std::allocator";
        case 'b': return "std::basic_string";
        case 's': return "std::string";
        case 't': return "std::";
        default:
                  return "std::???";
        }
        input++;
        OrkAssert(nameref < int(names.size()));
        return names[orkvector<std::string>::size_type(nameref)];
    } else if(isdigit(*input)) {
        int namelen = strtol(input, const_cast<char **>(&input), 10);
        std::string result;
		result.reserve(std::string::size_type(namelen));
        for(int i = 0; i < namelen; i++) {
            OrkAssert(*input);
            result += *input++;
        }
        return result;
    } else switch(*input++) {
    case 'i': return "int";
    case 'c': return "char";
    case 'f': return "float";
    case 's': return "short";
    case 'b': return "bool";
    case 'l': return "long";
    default: // flf: incomplete, I'm sure.
              return "???";
    }
}

static void GccDemangleRecurse(std::string &result, orkvector<std::string> &names, const char *&input)
{
    std::string subresult;
    if(strchr("IN", *input) == NULL) {
        do {
            subresult += ReadMangledName(input, names, subresult);
        } while(subresult[subresult.size()-1] == ':');
    } else switch(*input) {
    case 'I':
        input++;
        result += '<';
        while(*input != 'E') {
            OrkAssert(input && *input);
            GccDemangleRecurse(subresult, names, input);
            names.push_back(subresult);
            if(strchr("INE", *input) == NULL) subresult += ",";
            else if(*input == 'E') subresult += ">";
        }
        input++;
        break;
    case 'N':
        input++;
        while(*input != 'E') {
            OrkAssert(input && *input);
            GccDemangleRecurse(subresult, names, input);
            names.push_back(subresult);
            if(strchr("INE", *input) == NULL) subresult += "::";
        }
        input++;
        break;
    }
    result += subresult;
}

// Function prototype to make CW happy
std::string GccDemangle(const std::string& mangled);

///////////////////////////////////////////////////////////////////////////////

// Function prototype to make CW happy
std::string MethodIdNameStrip(const char* name);

namespace tool {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

OrkQtApp::OrkQtApp( int& argc, char** argv )
	: QApplication( argc, argv )
	, mpMainWindow(0)
{
	bool bcon = mIdleTimer.connect( & mIdleTimer, SIGNAL(timeout()), this, SLOT(OnTimer()));

	mIdleTimer.setInterval(5);
	mIdleTimer.setSingleShot(false);
	mIdleTimer.start();

}

///////////////////////////////////////////////////////////////////////////////

void OrkQtApp::OnTimer()
{
	OpqTest opqtest(&MainThreadOpQ());
	while(MainThreadOpQ().Process());
}

///////////////////////////////////////////////////////////////////////////////

void OrkQtApp::MocInit( void )
{
	OrkQtApp::Moc.AddSlot0( "OnTimer()", & OrkQtApp::OnTimer );
}

ImplementMoc( OrkQtApp, QApplication );

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ProxyStyle : public QStyle
{
public:

#if defined(_DARWIN)
	ProxyStyle(const QString &baseStyle) { style = QStyleFactory::create("Macintosh"); }
#elif defined(IX)
	ProxyStyle(const QString &baseStyle)
	{
		QStringList keys = QStyleFactory::keys();
		for( auto item = keys.begin(); item!=keys.end(); item++ )
		{
			QString val = *item;
			const char* as_str = val.toAscii().constData();
			printf( "stylefact<%s>\n", as_str );
		}
	 	style = QStyleFactory::create("Oxygen");
	 }
#endif
	////////////////////////////////
	void polish(QWidget *w) { style->polish(w); }
	////////////////////////////////
	void unpolish(QWidget *w) { style->unpolish(w); }
	////////////////////////////////
	int pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget *widget) const
	{
		return style->pixelMetric(metric, option, widget);
	}
	////////////////////////////////
	void drawPrimitive ( PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget ) const
	{
		style->drawPrimitive( element, option, painter, widget );
	}
	////////////////////////////////
	QSize sizeFromContents ( ContentsType type, const QStyleOption* option, const QSize& contentsSize, const QWidget* widget  ) const
	{
		return style->sizeFromContents( type, option, contentsSize, widget );
	}
	////////////////////////////////
	QPixmap generatedIconPixmap ( QIcon::Mode iconMode, const QPixmap& pixmap, const QStyleOption* option ) const
	{
		return style->generatedIconPixmap( iconMode, pixmap, option );
	}
	////////////////////////////////
	SubControl hitTestComplexControl ( ComplexControl control, const QStyleOptionComplex* option, const QPoint& position, const QWidget* widget ) const
	{
		return style->hitTestComplexControl( control, option, position, widget );
	}
	////////////////////////////////
	int styleHint ( StyleHint hint, const QStyleOption* option, const QWidget* widget, QStyleHintReturn* returnData ) const
	{
		return style->styleHint( hint, option, widget, returnData );
	}
	////////////////////////////////
	QRect subControlRect ( ComplexControl control, const QStyleOptionComplex* option, SubControl subControl, const QWidget* widget ) const
	{
		return style->subControlRect( control, option, subControl, widget );
	}
	////////////////////////////////
	QRect subElementRect ( SubElement element, const QStyleOption* option, const QWidget* widget ) const
	{
		return style->subElementRect( element, option, widget );
	}
	////////////////////////////////
	void 	drawComplexControl ( ComplexControl control, const QStyleOptionComplex* option, QPainter* painter, const QWidget* widget ) const
	{
		style->drawComplexControl( control, option, painter, widget );
	}
	void 	drawControl ( ControlElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget ) const
	{
		style->drawControl( element, option, painter, widget );
	}
	////////////////////////////////
	QIcon 	standardIcon ( StandardPixmap standardIcon, const QStyleOption* option, const QWidget* widget ) const
	{
		return style->standardIcon( standardIcon, option, widget );
	}
	////////////////////////////////
	QPixmap standardPixmap(StandardPixmap standardPixmap, const QStyleOption* opt, const QWidget* widget ) const 
	{
		return style->standardPixmap( standardPixmap, opt, widget );
	}
	////////////////////////////////

private:
	QStyle *style;
};

class OrkStyle : public ProxyStyle
{
public:
	OrkStyle(const QString &baseStyle) : ProxyStyle(baseStyle) {}

	int pixelMetric(PixelMetric metric,const QStyleOption* option, const QWidget *widget) const;
};

int OrkStyle::pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget *widget) const
{
	
	switch( metric )
	{
		case PM_SmallIconSize:
			return 32;
		case PM_SplitterWidth:
			return 6;
		case PM_SizeGripSize:
			return 8;
		case PM_DockWidgetSeparatorExtent:
			return 6;
		default:
			return ProxyStyle::pixelMetric(metric, option, widget);
	}
}
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct InputArgs
{
    int& argc;
    char **argv;

    InputArgs( int& ac, char** av )
    	: argc(ac)
    	, argv(av)
    {}
};

OrkQtApp* gpQtApplication = nullptr;

void* BootQtThreadImpl(void* arg_opaq )
{
	InputArgs *args = (InputArgs*) arg_opaq;

	Opq& mainthreadopq = ork::MainThreadOpQ();
  	OpqTest ot( &mainthreadopq );

	int iret = 0;

///////////////////////////////////////////////////////////////////////////////
#if defined( ORK_CONFIG_QT )
///////////////////////////////////////////////////////////////////////////////
	gpQtApplication = new OrkQtApp( args->argc, args->argv );

#if defined(IX)
	QStyle *MainStyle = new OrkStyle("Macintosh");

#else
	QStyle *MainStyle = QStyleFactory::create( "WindowsXP" );
#endif

	OrkAssert( MainStyle!=0 );
	
//	QPalette palette = MainStyle->standardPalette();
//	gpQtApplication->setPalette( palette );
//	gpQtApplication->setStyle( MainStyle );
	
	std::string AppClassName = CSystem::GetGlobalStringVariable( "ProjectApplicationClassName" );

	ork::lev2::CInputManager::GetRef();
	ork::lev2::AudioDevice* paudio = ork::lev2::AudioDevice::GetDevice();

	ent::EditorMainWindow MainWin(0, AppClassName, *gpQtApplication );
	ent::gEditorMainWindow = &MainWin;
	MainWin.showMaximized();

	gpQtApplication->mpMainWindow = & MainWin;

    file::Path fname;

    for( int i=0; i<args->argc; i++ )
        if( 0 == strcmp(args->argv[i],"-edit") && (i<args->argc-1) ) 
            fname = args->argv[++i];

    if( fname.IsFile() )
        MainWin.QueueLoadScene( fname.c_str() );

	iret = gpQtApplication->exec();

	ork::ent::DrawableBuffer::ClearAndSyncWriters();

	delete paudio;

	delete gpQtApplication;

    gpQtApplication = nullptr;

	///////////////////////////////////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////////////////////////////////
	return 0;	
}
int QtTest( int& argc, char **argv, bool bgamemode, bool bmenumode )
{
    InputArgs args(argc,argv);
    //BootQtThreadImpl((void*)& args);
    BootQtThreadImpl( & args );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace tool
} // namespace ork

void OrkGlobalDisableMousePointer()
{
    //QApplication::setOverrideCursor( QCursor( Qt::BlankCursor ) );
}
void OrkGlobalEnableMousePointer()
{
    //QApplication::restoreOverrideCursor();
}

