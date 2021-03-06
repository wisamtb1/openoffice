/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_xmlhelp.hxx"
#include "db.hxx"
#ifndef _VOS_DIAGNOSE_HXX_
#include <vos/diagnose.hxx>
#endif
#include <osl/thread.h>
#include <rtl/uri.hxx>
#include <osl/file.hxx>
#include <rtl/memory.h>
#include <com/sun/star/lang/Locale.hpp>
#include <rtl/ustrbuf.hxx>
#include "inputstream.hxx"
#include <algorithm>
#include <string.h>

// Extensible help
#include "com/sun/star/deployment/ExtensionManager.hpp"
#include "com/sun/star/deployment/thePackageManagerFactory.hpp"
#include <comphelper/processfactory.hxx>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/uno/XComponentContext.hpp> 
#include <com/sun/star/ucb/XCommandEnvironment.hpp> 
#include <com/sun/star/beans/Optional.hpp> 
#include <com/sun/star/beans/PropertyValue.hpp> 
#include <com/sun/star/beans/NamedValue.hpp> 
#include <com/sun/star/frame/XConfigManager.hpp> 
#include <com/sun/star/util/XMacroExpander.hpp>
#include <com/sun/star/uri/XUriReferenceFactory.hpp>
#include <com/sun/star/uri/XVndSunStarExpandUrl.hpp>
#include <com/sun/star/script/XInvocation.hpp>
#include <comphelper/locale.hxx>

#include <com/sun/star/awt/XToolkit.hpp>
#include <com/sun/star/awt/XExtendedToolkit.hpp>
#include <com/sun/star/awt/XWindowPeer.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>
#include <com/sun/star/awt/XTopWindow.hpp>

#include <l10ntools/compilehelp.hxx>
#include <comphelper/storagehelper.hxx>

#include "databases.hxx"
#include "urlparameter.hxx"

using namespace chelp;
using namespace com::sun::star;
using namespace com::sun::star::uno;
using namespace com::sun::star::io;
using namespace com::sun::star::container;
using namespace com::sun::star::i18n;
using namespace com::sun::star::lang;
using namespace com::sun::star::deployment;
using namespace com::sun::star::beans;


static rtl::OUString aSlash( rtl::OUString::createFromAscii( "/" ) );
static rtl::OUString aHelpFilesBaseName( rtl::OUString::createFromAscii( "help" ) );
static rtl::OUString aHelpMediaType( rtl::OUString::createFromAscii( "application/vnd.sun.star.help" ) );

rtl::OUString Databases::expandURL( const rtl::OUString& aURL )
{
	osl::MutexGuard aGuard( m_aMutex );
	rtl::OUString aRetURL = expandURL( aURL, m_xContext );
	return aRetURL;
}

rtl::OUString Databases::expandURL( const rtl::OUString& aURL, Reference< uno::XComponentContext > xContext )
{
	static Reference< util::XMacroExpander > xMacroExpander;
	static Reference< uri::XUriReferenceFactory > xFac;

	if( !xContext.is() )
		return rtl::OUString();

	if( !xMacroExpander.is() || !xFac.is() )
	{
		Reference< XMultiComponentFactory > xSMgr( xContext->getServiceManager(), UNO_QUERY );

		xFac = Reference< uri::XUriReferenceFactory >(
			xSMgr->createInstanceWithContext( rtl::OUString::createFromAscii(
			"com.sun.star.uri.UriReferenceFactory"), xContext ) , UNO_QUERY );
		if( !xFac.is() )
		{
			throw RuntimeException(
				::rtl::OUString::createFromAscii( "Databases::expand(), could not instatiate UriReferenceFactory." ),
				Reference< XInterface >() );
		}

		xMacroExpander = Reference< util::XMacroExpander >(
			xContext->getValueByName(
			::rtl::OUString::createFromAscii( "/singletons/com.sun.star.util.theMacroExpander" ) ),
			UNO_QUERY_THROW );
 	}

	rtl::OUString aRetURL = aURL;
	if( xMacroExpander.is() )
	{
		Reference< uri::XUriReference > uriRef;
		for (;;)
		{
			uriRef = Reference< uri::XUriReference >( xFac->parse( aRetURL ), UNO_QUERY );
			if ( uriRef.is() )
			{
				Reference < uri::XVndSunStarExpandUrl > sxUri( uriRef, UNO_QUERY );
				if( !sxUri.is() )
					break;

				aRetURL = sxUri->expand( xMacroExpander );
			}
		}
 	}
	return aRetURL;
}

Databases::Databases( sal_Bool showBasic,
					  const rtl::OUString& instPath,
					  const com::sun::star::uno::Sequence< rtl::OUString >& imagesZipPaths,
					  const rtl::OUString& productName,
					  const rtl::OUString& productVersion,
					  const rtl::OUString& styleSheet,
					  Reference< uno::XComponentContext > xContext )
	: m_xContext( xContext ),
	  m_bShowBasic(showBasic),
	  m_nErrorDocLength( 0 ),
	  m_pErrorDoc( 0 ),
	  m_nCustomCSSDocLength( 0 ),
	  m_pCustomCSSDoc( 0 ),
	  m_aCSS(styleSheet.toAsciiLowerCase()),
	  newProdName(rtl::OUString::createFromAscii( "$[officename]" ) ),
      newProdVersion(rtl::OUString::createFromAscii( "$[officeversion]" ) ),
      prodName( rtl::OUString::createFromAscii( "%PRODUCTNAME" ) ),
	  prodVersion( rtl::OUString::createFromAscii( "%PRODUCTVERSION" ) ),
	  vendName( rtl::OUString::createFromAscii( "%VENDORNAME" ) ),
	  vendVersion( rtl::OUString::createFromAscii( "%VENDORVERSION" ) ),
	  vendShort( rtl::OUString::createFromAscii( "%VENDORSHORT" ) ),
	  m_aImagesZipPaths( imagesZipPaths ),
	  m_nSymbolsStyle( 0 )
{
	m_xSMgr = Reference< XMultiComponentFactory >( m_xContext->getServiceManager(), UNO_QUERY );

	m_vAdd[0] = 12;
	m_vAdd[1] = 15;
	m_vAdd[2] = 11;
	m_vAdd[3] = 14;
	m_vAdd[4] = 12;
	m_vAdd[5] = 13;
    m_vAdd[6] = 16;

	m_vReplacement[0] = productName;
	m_vReplacement[1] = productVersion;
    // m_vReplacement[2...4] (vendorName/-Version/-Short) are empty strings
	m_vReplacement[5] = productName;
    m_vReplacement[6] = productVersion;
    
	setInstallPath( instPath );

	m_xSFA = Reference< ucb::XSimpleFileAccess >( 
		m_xSMgr->createInstanceWithContext( rtl::OUString::createFromAscii( "com.sun.star.ucb.SimpleFileAccess" ),
		m_xContext ), UNO_QUERY_THROW );
}

Databases::~Databases()
{
	// release stylesheet
	
	delete[] m_pCustomCSSDoc;
	
	// release errorDocument
	
	delete[] m_pErrorDoc;
    
	// unload the databases
	
	{
		// DatabasesTable 
		DatabasesTable::iterator it = m_aDatabases.begin();
		while( it != m_aDatabases.end() )
		{
            delete it->second;
            ++it;
		}
	}
	
	{
		//  ModInfoTable
	
		ModInfoTable::iterator it = m_aModInfo.begin();
		while( it != m_aModInfo.end() )
		{
			delete it->second;
			++it;
		}
	}
	
	{
		// KeywordInfoTable
        
		KeywordInfoTable::iterator it = m_aKeywordInfo.begin();
		while( it != m_aKeywordInfo.end() )
		{
			delete it->second;
			++it;
		}
	}
}

static bool impl_getZipFile(
        Sequence< rtl::OUString > & rImagesZipPaths,
        const rtl::OUString & rZipName,
        rtl::OUString & rFileName )
{
    const rtl::OUString *pPathArray = rImagesZipPaths.getArray();
    for ( int i = 0; i < rImagesZipPaths.getLength(); ++i )
    {
        rFileName = pPathArray[ i ];
        if ( rFileName.getLength() )
        {
            if ( 1 + rFileName.lastIndexOf( '/' ) != rFileName.getLength() )
            {
                rFileName += rtl::OUString::createFromAscii( "/" );
            }
            rFileName += rZipName;

            // test existence
            osl::DirectoryItem aDirItem;
            if ( osl::DirectoryItem::get( rFileName, aDirItem ) == osl::FileBase::E_None )
                return true;
        }
    }
    return false;
}

rtl::OString Databases::getImagesZipFileURL()
{
    //sal_Int16 nSymbolsStyle = SvtMiscOptions().GetCurrentSymbolsStyle();
    sal_Int16 nSymbolsStyle = 0;
    try
    {
        uno::Reference< lang::XMultiServiceFactory > xConfigProvider(
            m_xSMgr ->createInstanceWithContext(::rtl::OUString::createFromAscii("com.sun.star.configuration.ConfigurationProvider"), m_xContext), uno::UNO_QUERY_THROW);

        // set root path
        uno::Sequence < uno::Any > lParams(1);
        beans::PropertyValue                       aParam ;
        aParam.Name    = ::rtl::OUString::createFromAscii("nodepath");
        aParam.Value <<= ::rtl::OUString::createFromAscii("org.openoffice.Office.Common");
        lParams[0] = uno::makeAny(aParam);

        // open it
        uno::Reference< uno::XInterface > xCFG( xConfigProvider->createInstanceWithArguments(
                    ::rtl::OUString::createFromAscii("com.sun.star.configuration.ConfigurationAccess"),
                    lParams) );

        bool bChanged = false;
		uno::Reference< container::XHierarchicalNameAccess > xAccess(xCFG, uno::UNO_QUERY_THROW);
        uno::Any aResult = xAccess->getByHierarchicalName(::rtl::OUString::createFromAscii("Misc/SymbolSet"));
        if ( (aResult >>= nSymbolsStyle) && m_nSymbolsStyle != nSymbolsStyle )
		{
			m_nSymbolsStyle = nSymbolsStyle;
			bChanged = true;
		}

        if ( !m_aImagesZipFileURL.getLength() || bChanged )
        {
            rtl::OUString aImageZip, aSymbolsStyleName;
			aResult = xAccess->getByHierarchicalName(::rtl::OUString::createFromAscii("Misc/SymbolStyle"));
			aResult >>= aSymbolsStyleName;

            bool bFound = false;
            if ( aSymbolsStyleName.getLength() != 0 )
            {
                rtl::OUString aZipName = rtl::OUString::createFromAscii( "images_" );
                aZipName += aSymbolsStyleName;
                aZipName += rtl::OUString::createFromAscii( ".zip" );

                bFound = impl_getZipFile( m_aImagesZipPaths, aZipName, aImageZip );
            }

            if ( ! bFound )
                bFound = impl_getZipFile( m_aImagesZipPaths, rtl::OUString::createFromAscii( "images.zip" ), aImageZip );

            if ( ! bFound )
                aImageZip = rtl::OUString();

            m_aImagesZipFileURL = rtl::OUStringToOString(
                        rtl::Uri::encode(
                            aImageZip,
                            rtl_UriCharClassPchar,
                            rtl_UriEncodeIgnoreEscapes,
                            RTL_TEXTENCODING_UTF8 ), RTL_TEXTENCODING_UTF8 );
        }
    }
    catch ( NoSuchElementException const & )
    {
    }

    return m_aImagesZipFileURL;
}

void Databases::replaceName( rtl::OUString& oustring ) const
{
	sal_Int32 idx = -1,idx1 = -1,idx2 = -1,k = 0,off;
	bool cap = false;
	rtl::OUStringBuffer aStrBuf( 0 );
	
	while( true )
	{
        ++idx;
        idx1 = oustring.indexOf( sal_Unicode('%'),idx);
        idx2 = oustring.indexOf( sal_Unicode('$'),idx);
        
        if(idx1 == -1 && idx2 == -1)
            break;
        
        if(idx1 == -1)
            idx = idx2;
        else if(idx2 == -1)
            idx = idx1;
        else {
            // no index is zero
            if(idx1 < idx2)
                idx = idx1;
            else if(idx2 < idx1 )
                idx = idx2;
        }
        
		if( oustring.indexOf( prodName,idx ) == idx )
			off = PRODUCTNAME;
		else if( oustring.indexOf( prodVersion,idx ) == idx )
			off = PRODUCTVERSION;
		else if( oustring.indexOf( vendName,idx ) == idx )
			off = VENDORNAME;
		else if( oustring.indexOf( vendVersion,idx ) == idx )
			off = VENDORVERSION;
		else if( oustring.indexOf( vendShort,idx ) == idx )
			off = VENDORSHORT;
		else if( oustring.indexOf( newProdName,idx ) == idx )
			off = NEWPRODUCTNAME;
		else if( oustring.indexOf( newProdVersion,idx ) == idx )
			off = NEWPRODUCTVERSION;
		else
			off = -1;
		
		if( off != -1 )
		{
			if( ! cap )
			{
				cap = true;
				aStrBuf.ensureCapacity( 256 );
			}
			
			aStrBuf.append( &oustring.getStr()[k],idx - k );
			aStrBuf.append( m_vReplacement[off] );
			k = idx + m_vAdd[off];
		}
	}
	
	if( cap )
	{
		if( k < oustring.getLength() )
			aStrBuf.append( &oustring.getStr()[k],oustring.getLength()-k );
		oustring = aStrBuf.makeStringAndClear();
	}
}


	

rtl::OUString Databases::getInstallPathAsSystemPath()
{
	osl::MutexGuard aGuard( m_aMutex );
	
	if( ! m_aInstallDirectoryAsSystemPath.getLength() )
	{
#ifdef DBG_UTIL
		bool bla =
			osl::FileBase::E_None ==
			osl::FileBase::getSystemPathFromFileURL( m_aInstallDirectory,m_aInstallDirectoryAsSystemPath );
        VOS_ENSURE( bla,"HelpProvider, no installpath" );
#else                    
        osl::FileBase::getSystemPathFromFileURL( m_aInstallDirectory,m_aInstallDirectoryAsSystemPath );		
#endif
	}
	
	return m_aInstallDirectoryAsSystemPath;
}

rtl::OUString Databases::getInstallPathAsURL()
{
	osl::MutexGuard aGuard( m_aMutex );
	
  	return m_aInstallDirectory;
}


const std::vector< rtl::OUString >& Databases::getModuleList( const rtl::OUString& Language )
{
	if( m_avModules.size() == 0 )
	{
		rtl::OUString  fileName,dirName = getInstallPathAsURL() + processLang( Language );
		osl::Directory dirFile( dirName );

		osl::DirectoryItem aDirItem;			
		osl::FileStatus    aStatus( FileStatusMask_FileName );
				
		sal_Int32 idx;

		if( osl::FileBase::E_None != dirFile.open() )
			return m_avModules;
			
		while( dirFile.getNextItem( aDirItem ) == osl::FileBase::E_None && 
			   aDirItem.getFileStatus( aStatus ) == osl::FileBase::E_None )
		{
			if( ! aStatus.isValid( FileStatusMask_FileName ) )
				continue;
					
			fileName = aStatus.getFileName();
					
			// Check, whether fileName is of the form *.cfg
			idx = fileName.lastIndexOf(  sal_Unicode( '.' ) );
					
			if( idx == -1 )
				continue;
					
			const sal_Unicode* str = fileName.getStr();
					
			if( fileName.getLength() == idx + 4                   && 
				( str[idx + 1] == 'c' || str[idx + 1] == 'C' )    && 
				( str[idx + 2] == 'f' || str[idx + 2] == 'F' )    &&
				( str[idx + 3] == 'g' || str[idx + 3] == 'G' )    &&
				( fileName = fileName.copy(0,idx).toAsciiLowerCase() ).compareToAscii( "picture" ) != 0 ) {
			  if(! m_bShowBasic && fileName.compareToAscii("sbasic") == 0 )
				continue;
			  m_avModules.push_back( fileName );
			}
		}
	}
	return m_avModules;
}		



StaticModuleInformation* Databases::getStaticInformationForModule( const rtl::OUString& Module,
																   const rtl::OUString& Language )
{
	osl::MutexGuard aGuard( m_aMutex );

	rtl::OUString key = processLang(Language) + rtl::OUString::createFromAscii( "/" ) + Module;

	std::pair< ModInfoTable::iterator,bool > aPair = 
		m_aModInfo.insert( ModInfoTable::value_type( key,0 ) );		

	ModInfoTable::iterator it = aPair.first;
	
	if( aPair.second && ! it->second )
	{
		osl::File cfgFile( getInstallPathAsURL() +
						   key +
						   rtl::OUString::createFromAscii( ".cfg" ) );
		
		if( osl::FileBase::E_None != cfgFile.open( OpenFlag_Read ) )
			it->second = 0;
		else
		{
			sal_uInt32 pos = 0;
			sal_uInt64 nRead;
			sal_Char buffer[2048];
			sal_Unicode lineBuffer[1028];
			rtl::OUString fileContent;
			
			while( osl::FileBase::E_None == cfgFile.read( &buffer,2048,nRead ) && nRead )
				fileContent += rtl::OUString( buffer,sal_Int32( nRead ),RTL_TEXTENCODING_UTF8 );
			
			cfgFile.close();
			
			const sal_Unicode* str = fileContent.getStr();
			rtl::OUString current,lang_,program,startid,title,heading,fulltext;
            rtl::OUString order = rtl::OUString::createFromAscii( "1" );
			
			for( sal_Int32 i = 0;i < fileContent.getLength();i++ )
			{
				sal_Unicode ch = str[ i ];
				if( ch == sal_Unicode( '\n' ) || ch == sal_Unicode( '\r' ) )
				{
					if( pos )
					{
						current = rtl::OUString( lineBuffer,pos );
						
						if( current.compareToAscii( "Title",5 ) == 0 )
						{
							title = current.copy( current.indexOf(sal_Unicode( '=' ) ) + 1 );
						}
						else if( current.compareToAscii( "Start",5 ) == 0 )
						{
							startid = current.copy( current.indexOf('=') + 1 );
						}
						else if( current.compareToAscii( "Language",8 ) == 0 )
						{
							lang_ = current.copy( current.indexOf('=') + 1 );
						}
						else if( current.compareToAscii( "Program",7 ) == 0 )
						{
							program = current.copy( current.indexOf('=') + 1 );
						}
						else if( current.compareToAscii( "Heading",7 ) == 0 )
						{
							heading = current.copy( current.indexOf('=') + 1 );
						}
						else if( current.compareToAscii( "FullText",8 ) == 0 )
						{
							fulltext = current.copy( current.indexOf('=') + 1 );
						}
						else if( current.compareToAscii( "Order",5 ) == 0 )
						{
							order = current.copy( current.indexOf('=') + 1 );
						}
					}
					pos = 0;
				}
				else
					lineBuffer[ pos++ ] = ch;
			}
			replaceName( title );
			it->second = new StaticModuleInformation( title,
													  startid,
													  program,
													  heading,
													  fulltext,
													  order );			
		}
	}
	
	return it->second;
}




rtl::OUString Databases::processLang( const rtl::OUString& Language )
{
	osl::MutexGuard aGuard( m_aMutex );

	rtl::OUString ret;
	LangSetTable::iterator it = m_aLangSet.find( Language );
	
	if( it == m_aLangSet.end() )
	{
		sal_Int32 idx;
		osl::DirectoryItem aDirItem;
		
		if( osl::FileBase::E_None == osl::DirectoryItem::get( getInstallPathAsURL() + Language,aDirItem ) )
		{
			ret = Language;
			m_aLangSet[ Language ] = ret;
		}
		else if( ( ( idx = Language.indexOf( '-' ) ) != -1 ||
				   ( idx = Language.indexOf( '_' ) ) != -1 ) &&
					osl::FileBase::E_None == osl::DirectoryItem::get( getInstallPathAsURL() + Language.copy( 0,idx ),
																   aDirItem ) )
		{
			ret = Language.copy( 0,idx );
			m_aLangSet[ Language ] = ret;
		}
	}
	else
		ret = it->second;

	return ret;
}		


rtl::OUString Databases::country( const rtl::OUString& Language )
{
	sal_Int32 idx;
	if( ( idx = Language.indexOf( '-' ) ) != -1 ||
		( idx = Language.indexOf( '_' ) ) != -1 )
		return Language.copy( 1+idx );
	
	return rtl::OUString();
}



helpdatafileproxy::Hdf* Databases::getHelpDataFile( const rtl::OUString& Database,
							const rtl::OUString& Language, bool helpText,
							const rtl::OUString* pExtensionPath )
{
	if( ! Database.getLength() || ! Language.getLength() )
		return 0;
	
	osl::MutexGuard aGuard( m_aMutex );


	rtl::OUString aFileExt( rtl::OUString::createFromAscii( helpText ? ".ht" : ".db" ) );
	rtl::OUString dbFileName = aSlash + Database + aFileExt;
	rtl::OUString key;
	if( pExtensionPath == NULL )
		key = processLang( Language ) + dbFileName;
	else
		key = *pExtensionPath + Language + dbFileName;		// make unique, don't change language

	std::pair< DatabasesTable::iterator,bool > aPair = 
		m_aDatabases.insert( DatabasesTable::value_type( key,0 ) );
	
	DatabasesTable::iterator it = aPair.first;	
	
	if( aPair.second && ! it->second )
	{
		helpdatafileproxy::Hdf* pHdf = 0;
		
		rtl::OUString fileURL;
		if( pExtensionPath )
            fileURL = expandURL(*pExtensionPath) + Language + dbFileName;
		else
            fileURL = getInstallPathAsURL() + key;

		rtl::OUString fileNameHDFHelp( fileURL );
        //Extensions always use the new format
		if( pExtensionPath != NULL )
			fileNameHDFHelp += rtl::OUString::createFromAscii( "_" );
        //SimpleFileAccess takes file URLs as arguments!!! Using filenames works accidentally but
        //fails for example when using long path names on Windows (starting with \\?\)
		if( m_xSFA->exists( fileNameHDFHelp ) )
		{
			pHdf = new helpdatafileproxy::Hdf( fileNameHDFHelp, m_xSFA );
		}
		
		it->second = pHdf;
	}

	return it->second;
}

Reference< XCollator >
Databases::getCollator( const rtl::OUString& Language,
						const rtl::OUString& System )
{
	(void)System;

	rtl::OUString key = Language;
	
	osl::MutexGuard aGuard( m_aMutex );
	
	CollatorTable::iterator it =
		m_aCollatorTable.insert( CollatorTable::value_type( key,0 ) ).first;
	
	if( ! it->second.is() )
	{
		it->second = 
			Reference< XCollator > ( 
				m_xSMgr->createInstanceWithContext( rtl::OUString::createFromAscii( "com.sun.star.i18n.Collator" ),
				m_xContext ), UNO_QUERY );
		rtl::OUString langStr = processLang(Language);
		rtl::OUString countryStr = country(Language);
		if( !countryStr.getLength() )
		{
			if( langStr.compareToAscii("de") == 0 )
				countryStr = rtl::OUString::createFromAscii("DE");
			else if( langStr.compareToAscii("en") == 0 )
				countryStr = rtl::OUString::createFromAscii("US");
			else if( langStr.compareToAscii("es") == 0 )
				countryStr = rtl::OUString::createFromAscii("ES");
			else if( langStr.compareToAscii("it") == 0 )
				countryStr = rtl::OUString::createFromAscii("IT");
			else if( langStr.compareToAscii("fr") == 0 )
				countryStr = rtl::OUString::createFromAscii("FR");
			else if( langStr.compareToAscii("sv") == 0 )
				countryStr = rtl::OUString::createFromAscii("SE");
			else if( langStr.compareToAscii("ja") == 0 )
				countryStr = rtl::OUString::createFromAscii("JP");
			else if( langStr.compareToAscii("ko") == 0 )
				countryStr = rtl::OUString::createFromAscii("KR");
		}
		it->second->loadDefaultCollator(  Locale( langStr,
												  countryStr,
												  rtl::OUString() ),
										  0 );
	}
	
	return it->second;
}



namespace chelp {
    
    struct KeywordElementComparator
    {
        KeywordElementComparator( const Reference< XCollator >& xCollator )
            : m_xCollator( xCollator )
        { }
        
        bool operator()( const KeywordInfo::KeywordElement& la,
                         const KeywordInfo::KeywordElement& ra ) const
        {
            const rtl::OUString& l = la.key;
            const rtl::OUString& r = ra.key;
            
            bool ret;
            
            if( m_xCollator.is() )
            {
                sal_Int32 l1 = l.indexOf( sal_Unicode( ';' ) );
                sal_Int32 l3 = ( l1 == -1 ? l.getLength() : l1 );
        
                sal_Int32 r1 = r.indexOf( sal_Unicode( ';' ) );
                sal_Int32 r3 = ( r1 == -1 ? r.getLength() : r1 );
                
                sal_Int32 c1 = m_xCollator->compareSubstring( l,0,l3,r,0,r3 );
                
                if( c1 == +1 )
                    ret = false;
                else if( c1 == 0 )
                {
                    sal_Int32 l2 = l.getLength() - l1 - 1;
                    sal_Int32 r2 = r.getLength() - r1 - 1;
                    ret = ( m_xCollator->compareSubstring( l,1+l1,l2,r,1+r1,r2 ) < 0 );
                }
                else
                    ret = true;
            }
            else
                ret = bool( l < r );
            
            return ret;
        }
        
        Reference< XCollator > m_xCollator;
    }; // end struct KeywordElementComparator
    
}



KeywordInfo::KeywordElement::KeywordElement( Databases *pDatabases,
											 helpdatafileproxy::Hdf* pHdf,
											 rtl::OUString& ky,
											 rtl::OUString& data )
	: key( ky )
{
	pDatabases->replaceName( key );
	init( pDatabases,pHdf,data );
}



void KeywordInfo::KeywordElement::init( Databases *pDatabases,helpdatafileproxy::Hdf* pHdf,const rtl::OUString& ids )
{
	const sal_Unicode* idstr = ids.getStr();
	std::vector< rtl::OUString > id,anchor;
	int idx = -1,k;
	while( ( idx = ids.indexOf( ';',k = ++idx ) ) != -1 )
	{
		int h = ids.indexOf( sal_Unicode( '#' ),k );
		if( h < idx )
		{
			// found an anchor
			id.push_back( rtl::OUString( &idstr[k],h-k ) );
			anchor.push_back( rtl::OUString( &idstr[h+1],idx-h-1 ) );
		}
		else
		{
			id.push_back( rtl::OUString( &idstr[k],idx-k ) );
			anchor.push_back( rtl::OUString() );
		}
	}
	
	listId.realloc( id.size() );
	listAnchor.realloc( id.size() );
	listTitle.realloc( id.size() );

	int nSize = 0;
	const sal_Char* pData = NULL;
	const sal_Char pEmpty[] = "";

	for( sal_uInt32 i = 0; i < id.size(); ++i )
	{
		listId[i] = id[i];
		listAnchor[i] = anchor[i];

		nSize = 0;
		pData = pEmpty;
		if( pHdf )
		{
			rtl::OString idi( id[i].getStr(),id[i].getLength(),RTL_TEXTENCODING_UTF8 );		
			helpdatafileproxy::HDFData aHDFData;
			bool bSuccess = pHdf->getValueForKey( idi, aHDFData );
			if( bSuccess )
			{
				nSize = aHDFData.getSize();
				pData = aHDFData.getData();
			}
		}
		
		DbtToStringConverter converter( pData, nSize );
		
		rtl::OUString title = converter.getTitle();
		pDatabases->replaceName( title );
		listTitle[i] = title;
	}
}



KeywordInfo::KeywordInfo( const std::vector< KeywordElement >& aVec )
	: listKey( aVec.size() ),
	  listId( aVec.size() ),
	  listAnchor( aVec.size() ),
	  listTitle( aVec.size() )
{
    for( unsigned int i = 0; i < aVec.size(); ++i )
	{
		listKey[i] = aVec[i].key;
		listId[i] = aVec[i].listId;
		listAnchor[i] = aVec[i].listAnchor;
		listTitle[i] = aVec[i].listTitle;
	}
}

bool Databases::checkModuleMatchForExtension
	( const rtl::OUString& Database, const rtl::OUString& doclist )
{
	bool bBelongsToDatabase = true;

	// Analyse doclist string to find module assignments
	bool bFoundAtLeastOneModule = false;
	bool bModuleMatch = false;
	sal_Int32 nLen = doclist.getLength();
	sal_Int32 nLastFound = doclist.lastIndexOf( sal_Unicode(';') );
	if( nLastFound == -1 )
		nLastFound = nLen;
	const sal_Unicode* pStr = doclist.getStr();
	sal_Int32 nFound = doclist.lastIndexOf( sal_Unicode('_') );
	while( nFound != -1 )
	{
		// Simple optimization, stop if '_' is followed by "id"
		if( nLen - nFound > 2 )
		{
			if( pStr[ nFound + 1 ] == sal_Unicode('i') &&
				pStr[ nFound + 2 ] == sal_Unicode('d') )
					break;
		}

		rtl::OUString aModule = doclist.copy( nFound + 1, nLastFound - nFound - 1 );
		std::vector< rtl::OUString >::iterator result = std::find( m_avModules.begin(), m_avModules.end(), aModule );
		if( result != m_avModules.end() )
		{
			bFoundAtLeastOneModule = true;
			if( Database == aModule )
			{
				bModuleMatch = true;
				break;
			}
		}

		nLastFound = nFound;
		if( nLastFound == 0 )
			break;
		nFound = doclist.lastIndexOf( sal_Unicode('_'), nLastFound - 1 );
	}

	if( bFoundAtLeastOneModule && !bModuleMatch )
		bBelongsToDatabase = false;

	return bBelongsToDatabase;
}


KeywordInfo* Databases::getKeyword( const rtl::OUString& Database,
									const rtl::OUString& Language )
{
	osl::MutexGuard aGuard( m_aMutex );
	
	rtl::OUString key = processLang(Language) + rtl::OUString::createFromAscii( "/" ) + Database;
	
    std::pair< KeywordInfoTable::iterator,bool > aPair = 
		m_aKeywordInfo.insert( KeywordInfoTable::value_type( key,0 ) );		
	
	KeywordInfoTable::iterator it = aPair.first;	
	
	if( aPair.second && ! it->second )
	{
		std::vector<KeywordInfo::KeywordElement> aVector;

		KeyDataBaseFileIterator aDbFileIt( m_xContext, *this, Database, Language );
		rtl::OUString fileURL;
		bool bExtension = false;
		while( (fileURL = aDbFileIt.nextDbFile( bExtension )).getLength() > 0 )
		{
			rtl::OUString fileNameHDFHelp( fileURL );
			if( bExtension )
				fileNameHDFHelp += rtl::OUString::createFromAscii( "_" );
			if( m_xSFA->exists( fileNameHDFHelp ) )
			{
				helpdatafileproxy::Hdf aHdf( fileNameHDFHelp, m_xSFA );

				helpdatafileproxy::HDFData aKey;
				helpdatafileproxy::HDFData aValue;
				if( aHdf.startIteration() )
				{
				    helpdatafileproxy::Hdf* pHdf = getHelpDataFile( Database,Language );
					if( pHdf != NULL )
					{
						bool bOptimizeForPerformance = true;
						pHdf->releaseHashMap();
						pHdf->createHashMap( bOptimizeForPerformance );
					}

					while( aHdf.getNextKeyAndValue( aKey, aValue ) )
					{
						rtl::OUString keyword( aKey.getData(), aKey.getSize(),
											   RTL_TEXTENCODING_UTF8 );
						rtl::OUString doclist( aValue.getData(), aValue.getSize(),
											   RTL_TEXTENCODING_UTF8 );

						bool bBelongsToDatabase = true;
						if( bExtension )
							bBelongsToDatabase = checkModuleMatchForExtension( Database, doclist );

						if( !bBelongsToDatabase )
							continue;
					
						aVector.push_back( KeywordInfo::KeywordElement( this,
																		pHdf,
																		keyword,
																		doclist ) );
					}
					aHdf.stopIteration();

					if( pHdf != NULL )
						pHdf->releaseHashMap();
				}
			}
		}

		// sorting
		Reference< XCollator > xCollator = getCollator( Language,rtl::OUString());
		KeywordElementComparator aComparator( xCollator );
		std::sort(aVector.begin(),aVector.end(),aComparator);
		
		KeywordInfo* pInfo = it->second = new KeywordInfo( aVector );
		(void)pInfo;
	}
	
	return it->second;
}

Reference< XHierarchicalNameAccess > Databases::jarFile( const rtl::OUString& jar,
														 const rtl::OUString& Language )
{
	if( ! jar.getLength() ||
		! Language.getLength() )
    {
		return Reference< XHierarchicalNameAccess >( 0 );
    }
	rtl::OUString key = processLang(Language) + aSlash + jar;

	osl::MutexGuard aGuard( m_aMutex );
	
	ZipFileTable::iterator it =
		m_aZipFileTable.insert( ZipFileTable::value_type( key,Reference< XHierarchicalNameAccess >(0) ) ).first;
	
	if( ! it->second.is() )
	{
        rtl::OUString zipFile;
		try
		{
			// Extension jar file? Search for ?
	        sal_Int32 nQuestionMark1 = jar.indexOf( sal_Unicode('?') );
	        sal_Int32 nQuestionMark2 = jar.lastIndexOf( sal_Unicode('?') );
			if( nQuestionMark1 != -1 && nQuestionMark2 != -1 && nQuestionMark1 != nQuestionMark2 )
			{
				::rtl::OUString aExtensionPath = jar.copy( nQuestionMark1 + 1, nQuestionMark2 - nQuestionMark1 - 1 );
				::rtl::OUString aPureJar = jar.copy( nQuestionMark2 + 1 );

				rtl::OUStringBuffer aStrBuf;
				aStrBuf.append( aExtensionPath );
				aStrBuf.append( aSlash );
				aStrBuf.append( aPureJar );

				zipFile = expandURL( aStrBuf.makeStringAndClear() );
			}
			else
			{
	            zipFile = getInstallPathAsURL() + key;
			}

            Sequence< Any > aArguments( 2 );
            
            XInputStream_impl* p = new XInputStream_impl( zipFile );
            if( p->CtorSuccess() )
            {
                Reference< XInputStream > xInputStream( p );
                aArguments[ 0 ] <<= xInputStream;
            }
            else
            {
                delete p;
                aArguments[ 0 ] <<= zipFile;
            }
			
            // let ZipPackage be used ( no manifest.xml is required )
            beans::NamedValue aArg;
            aArg.Name = ::rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "StorageFormat" ) );
            aArg.Value <<= ZIP_STORAGE_FORMAT_STRING;
            aArguments[ 1 ] <<= aArg;

			Reference< XInterface > xIfc
				= m_xSMgr->createInstanceWithArgumentsAndContext(
					rtl::OUString::createFromAscii(
						"com.sun.star.packages.comp.ZipPackage" ),
					aArguments, m_xContext );
			
			if ( xIfc.is() )
			{
				it->second = Reference< XHierarchicalNameAccess >( xIfc, UNO_QUERY );
				
				VOS_ENSURE( it->second.is(),
							"ContentProvider::createPackage - "
							"Got no hierarchical name access!" );
				
			}            
		}
		catch ( RuntimeException & )
		{
		}
		catch ( Exception & )
		{
		}
	}
	
	return it->second;
}

Reference< XHierarchicalNameAccess > Databases::findJarFileForPath
	( const rtl::OUString& jar, const rtl::OUString& Language,
	  const rtl::OUString& path, rtl::OUString* o_pExtensionPath,
      rtl::OUString* o_pExtensionRegistryPath )
{
	Reference< XHierarchicalNameAccess > xNA;
	if( ! jar.getLength() ||
		! Language.getLength() )
    {
		return xNA;
    }

	JarFileIterator aJarFileIt( m_xContext, *this, jar, Language );
	Reference< XHierarchicalNameAccess > xTestNA;
	Reference< deployment::XPackage > xParentPackageBundle;
	while( (xTestNA = aJarFileIt.nextJarFile( xParentPackageBundle, o_pExtensionPath, o_pExtensionRegistryPath )).is() )
	{
		if( xTestNA.is() && xTestNA->hasByHierarchicalName( path ) )
		{
			bool bSuccess = true;
			if( xParentPackageBundle.is() )
			{
				rtl::OUString aIdentifierInPath;
				sal_Int32 nFindSlash = path.indexOf( '/' );
				if( nFindSlash != -1 )
					aIdentifierInPath = path.copy( 0, nFindSlash );

				beans::Optional<rtl::OUString> aIdentifierOptional = xParentPackageBundle->getIdentifier();
				if( aIdentifierInPath.getLength() && aIdentifierOptional.IsPresent )
				{
					rtl::OUString aUnencodedIdentifier = aIdentifierOptional.Value;
					rtl::OUString aIdentifier = rtl::Uri::encode( aUnencodedIdentifier, 
						rtl_UriCharClassPchar, rtl_UriEncodeIgnoreEscapes, RTL_TEXTENCODING_UTF8 );

					if( !aIdentifierInPath.equals( aIdentifier ) )
					{
						// path does not start with extension identifier -> ignore
						bSuccess = false;
					} 
				}
				else
				{
					// No identifier -> ignore
					bSuccess = false;
				} 
			}

			if( bSuccess )
			{
				xNA = xTestNA;
				break;
			}
		}
	}
	
	return xNA;
}

void Databases::popupDocument( URLParameter* urlPar,char **buffer,int *byteCount )
{
	const char* pop1 =
		" <html>                                                                "
		" <head>                                                                "
		" <help:css-file-link xmlns:help=\"http://openoffice.org/2000/help\"/>  "
		" </head>                                                               "
		" <body>                                                                "
		" <help:popup-cut Id=\"";
	const sal_Int32 l1 = strlen( pop1 );
	
	const char* pop3 = "\" Eid=\"";
	const sal_Int32 l3 = strlen( pop3 );

	const char* pop5 = 
		"\" xmlns:help=\"http://openoffice.org/2000/help\"></help:popup-cut>  "
		" </body>                                                             "
		" </html>";
	const sal_Int32 l5 = strlen( pop5 );
	sal_Int32 l2,l4;
	
	rtl::OUString val = urlPar->get_id();
	rtl::OString pop2O( val.getStr(),l2 = val.getLength(),RTL_TEXTENCODING_UTF8 );
	const char* pop2 = pop2O.getStr();

	val = urlPar->get_eid();
	rtl::OString pop4O( val.getStr(),l4 = val.getLength(),RTL_TEXTENCODING_UTF8 );
	const char* pop4 = pop4O.getStr();
	
	(*byteCount) = l1 + l2 + l3 + l4 + l5;
		
	*buffer = new char[ 1+*byteCount ];

	rtl_copyMemory( *buffer,pop1,l1 );
	rtl_copyMemory( *buffer+l1,pop2,l2 );
	rtl_copyMemory( *buffer+(l1+l2),pop3,l3 );
	rtl_copyMemory( *buffer+(l1+l2+l3),pop4,l4 );
	rtl_copyMemory( *buffer+(l1+l2+l3+l4),pop5,l5 );
	(*buffer)[*byteCount] = 0;
}


void Databases::changeCSS(const rtl::OUString& newStyleSheet)
{
	m_aCSS = newStyleSheet.toAsciiLowerCase();
	delete[] m_pCustomCSSDoc, m_pCustomCSSDoc = 0,m_nCustomCSSDocLength = 0;
}



void Databases::cascadingStylesheet( const rtl::OUString& Language,
									 char** buffer,
									 int* byteCount )
{	
	if( ! m_pCustomCSSDoc )
	{
		int retry = 2;
		bool error = true;
		rtl::OUString fileURL;

        sal_Bool bHighContrastMode = sal_False;
        rtl::OUString aCSS( m_aCSS );
        if ( aCSS.compareToAscii( "default" ) == 0 )
        {
            // #i50760: "default" needs to adapt HC mode
            uno::Reference< awt::XToolkit > xToolkit = uno::Reference< awt::XToolkit >(
                    ::comphelper::getProcessServiceFactory()->createInstance( rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.awt.Toolkit" ) ) ), uno::UNO_QUERY );
            if ( xToolkit.is() )
            {
                uno::Reference< awt::XExtendedToolkit > xExtToolkit( xToolkit, uno::UNO_QUERY );
                if ( xExtToolkit.is() )
                {
                    uno::Reference< awt::XTopWindow > xTopWindow = xExtToolkit->getActiveTopWindow();
                    if ( xTopWindow.is() )
                    {
                        uno::Reference< awt::XVclWindowPeer > xVclWindowPeer( xTopWindow, uno::UNO_QUERY );
                        if ( xVclWindowPeer.is() )
                        {
                            uno::Any aHCMode = xVclWindowPeer->getProperty( rtl::OUString::createFromAscii( "HighContrastMode" ) );
                            if ( ( aHCMode >>= bHighContrastMode ) && bHighContrastMode )
                                aCSS = rtl::OUString::createFromAscii( "highcontrastblack" );
                        }
                    }
                }
            }
        }
		
		while( error && retry )
		{
		    
			if( retry == 2 )
				fileURL = 
					getInstallPathAsURL()  +
					processLang( Language )       +
					rtl::OUString::createFromAscii( "/" ) +
					aCSS +
					rtl::OUString::createFromAscii( ".css" );
			else if( retry == 1 )
				fileURL = 
					getInstallPathAsURL()  +
					aCSS +
					rtl::OUString::createFromAscii( ".css" );
			
			osl::DirectoryItem aDirItem;
			osl::File aFile( fileURL );
			osl::FileStatus aStatus( FileStatusMask_FileSize );
			
			if( osl::FileBase::E_None == osl::DirectoryItem::get( fileURL,aDirItem ) &&
				osl::FileBase::E_None == aFile.open( OpenFlag_Read )                 &&
				osl::FileBase::E_None == aDirItem.getFileStatus( aStatus ) )
			{
				m_nCustomCSSDocLength = int( aStatus.getFileSize() );
				m_pCustomCSSDoc = new char[ 1 + m_nCustomCSSDocLength ];
				m_pCustomCSSDoc[ m_nCustomCSSDocLength ] = 0;
				sal_uInt64 a = m_nCustomCSSDocLength,b = m_nCustomCSSDocLength;
				aFile.read( m_pCustomCSSDoc,a,b );
				aFile.close();
				error = false;
			}

			--retry;
			if ( !retry && error && bHighContrastMode )
			{
			    // fall back to default css
			    aCSS = rtl::OUString::createFromAscii( "default" );
			    retry = 2;
			    bHighContrastMode = sal_False;
			}
		}
		
		if( error )
		{
			m_nCustomCSSDocLength = 0;
			m_pCustomCSSDoc = new char[ 1 ]; // Initialize with 1 to avoid gcc compiler warning
		}
	}
	
	*byteCount = m_nCustomCSSDocLength;
	*buffer = new char[ 1 + *byteCount ];
	(*buffer)[*byteCount] = 0;
	rtl_copyMemory( *buffer,m_pCustomCSSDoc,m_nCustomCSSDocLength );
	
}


void Databases::setActiveText( const rtl::OUString& Module,
							   const rtl::OUString& Language,
							   const rtl::OUString& Id,
							   char** buffer,
							   int* byteCount )
{
	DataBaseIterator aDbIt( m_xContext, *this, Module, Language, true );

	// #i84550 Cache information about failed ids
	rtl::OString id( Id.getStr(),Id.getLength(),RTL_TEXTENCODING_UTF8 );
	EmptyActiveTextSet::iterator it = m_aEmptyActiveTextSet.find( id );
	bool bFoundAsEmpty = ( it != m_aEmptyActiveTextSet.end() );
	helpdatafileproxy::HDFData aHDFData;

	int nSize = 0;
	const sal_Char* pData = NULL;

	bool bSuccess = false;
	if( !bFoundAsEmpty )
	{
		helpdatafileproxy::Hdf* pHdf = 0;
		while( !bSuccess && (pHdf = aDbIt.nextHdf()) != NULL )
		{
			bSuccess = pHdf->getValueForKey( id, aHDFData );
			nSize = aHDFData.getSize();
			pData = aHDFData.getData();
		}
	}

	if( bSuccess )
	{
		// ensure existence of tmp after for 
		rtl::OString tmp;
		for( int i = 0; i < nSize; ++i )
			if( pData[i] == '%' || pData[i] == '$' )
			{
				// need of replacement
				rtl::OUString temp = rtl::OUString( pData, nSize, RTL_TEXTENCODING_UTF8 );
				replaceName( temp );
				tmp = rtl::OString( temp.getStr(),
									temp.getLength(),
									RTL_TEXTENCODING_UTF8 );
				nSize = tmp.getLength();
				pData = tmp.getStr();
				break;
			}

		*byteCount = nSize;
		*buffer = new char[ 1 + nSize ];
		(*buffer)[nSize] = 0;
		rtl_copyMemory( *buffer, pData, nSize );
	}
	else
	{
		*byteCount = 0;
		*buffer = new char[1]; // Initialize with 1 to avoid compiler warnings
		if( !bFoundAsEmpty )
			m_aEmptyActiveTextSet.insert( id );
	}
}


void Databases::setInstallPath( const rtl::OUString& aInstDir )
{
	osl::MutexGuard aGuard( m_aMutex );
	    
    osl::FileBase::getFileURLFromSystemPath( aInstDir,m_aInstallDirectory );
        //TODO: check returned error code
	
	if( m_aInstallDirectory.lastIndexOf( sal_Unicode( '/' ) ) != m_aInstallDirectory.getLength() - 1 )
		m_aInstallDirectory += rtl::OUString::createFromAscii( "/" );
	
	m_aInstallDirectoryWithoutEncoding = rtl::Uri::decode( m_aInstallDirectory,
														   rtl_UriDecodeWithCharset,
														   RTL_TEXTENCODING_UTF8 );
}


//===================================================================
// class ExtensionIteratorBase

ExtensionHelpExistanceMap ExtensionIteratorBase::aHelpExistanceMap;

ExtensionIteratorBase::ExtensionIteratorBase( Reference< XComponentContext > xContext, 
	Databases& rDatabases, const rtl::OUString& aInitialModule, const rtl::OUString& aLanguage )
		: m_xContext( xContext )
		, m_rDatabases( rDatabases )
		, m_eState( INITIAL_MODULE )
		, m_aInitialModule( aInitialModule )
		, m_aLanguage( aLanguage )
{
	init();
}

ExtensionIteratorBase::ExtensionIteratorBase( Databases& rDatabases,
	const rtl::OUString& aInitialModule, const rtl::OUString& aLanguage )
		: m_rDatabases( rDatabases )
		, m_eState( INITIAL_MODULE )
		, m_aInitialModule( aInitialModule )
		, m_aLanguage( aLanguage )
{
	init();
}

void ExtensionIteratorBase::init()
{
	if( !m_xContext.is() )
	{
		Reference< XMultiServiceFactory > xFactory = comphelper::getProcessServiceFactory();
		Reference< XPropertySet > xProps( xFactory, UNO_QUERY );
		OSL_ASSERT( xProps.is() );
		if (xProps.is())
		{
			xProps->getPropertyValue(
				::rtl::OUString( RTL_CONSTASCII_USTRINGPARAM("DefaultContext") ) ) >>= m_xContext;
			OSL_ASSERT( m_xContext.is() );
		}
	}
	if( !m_xContext.is() )
	{
		throw RuntimeException(
			::rtl::OUString::createFromAscii( "ExtensionIteratorBase::init(), no XComponentContext" ),
			Reference< XInterface >() );
	}

	Reference< XMultiComponentFactory > xSMgr( m_xContext->getServiceManager(), UNO_QUERY );
	m_xSFA = Reference< ucb::XSimpleFileAccess >( 
		xSMgr->createInstanceWithContext( rtl::OUString::createFromAscii( "com.sun.star.ucb.SimpleFileAccess" ),
		m_xContext ), UNO_QUERY_THROW );

	m_bUserPackagesLoaded = false;
	m_bSharedPackagesLoaded = false;
    m_bBundledPackagesLoaded = false;
	m_iUserPackage = 0;
	m_iSharedPackage = 0;
    m_iBundledPackage = 0;
}

Reference< deployment::XPackage > ExtensionIteratorBase::implGetHelpPackageFromPackage
	( Reference< deployment::XPackage > xPackage, Reference< deployment::XPackage >& o_xParentPackageBundle )
{
	o_xParentPackageBundle.clear();

	Reference< deployment::XPackage > xHelpPackage;
	if( !xPackage.is() )
		return xHelpPackage;

	// #i84550 Cache information about help content in extension
	rtl::OUString aExtensionPath = xPackage->getURL();
	ExtensionHelpExistanceMap::iterator it = aHelpExistanceMap.find( aExtensionPath );
	bool bFound = ( it != aHelpExistanceMap.end() );
	bool bHasHelp = bFound ? it->second : false;
	if( bFound && !bHasHelp )
		return xHelpPackage;

	// Check if parent package is registered
    beans::Optional< beans::Ambiguous<sal_Bool> > option( xPackage->isRegistered
		( Reference<task::XAbortChannel>(), Reference<ucb::XCommandEnvironment>() ) );
	bool bRegistered = false;
    if( option.IsPresent )
    {
        beans::Ambiguous<sal_Bool> const & reg = option.Value;
        if( !reg.IsAmbiguous && reg.Value )
			bRegistered = true;
    }
	if( bRegistered )
	{
		if( xPackage->isBundle() )
		{
			Sequence< Reference< deployment::XPackage > > aPkgSeq = xPackage->getBundle
				( Reference<task::XAbortChannel>(), Reference<ucb::XCommandEnvironment>() );
			sal_Int32 nPkgCount = aPkgSeq.getLength();
			const Reference< deployment::XPackage >* pSeq = aPkgSeq.getConstArray();
			for( sal_Int32 iPkg = 0 ; iPkg < nPkgCount ; ++iPkg )
			{
				const Reference< deployment::XPackage > xSubPkg = pSeq[ iPkg ];
				const Reference< deployment::XPackageTypeInfo > xPackageTypeInfo = xSubPkg->getPackageType();
				rtl::OUString aMediaType = xPackageTypeInfo->getMediaType();
				if( aMediaType.equals( aHelpMediaType ) )
				{
					xHelpPackage = xSubPkg;
					o_xParentPackageBundle = xPackage;
					break;
				}
			}
		}
		else
		{
			const Reference< deployment::XPackageTypeInfo > xPackageTypeInfo = xPackage->getPackageType();
			rtl::OUString aMediaType = xPackageTypeInfo->getMediaType();
			if( aMediaType.equals( aHelpMediaType ) )
				xHelpPackage = xPackage;
		}
	}

	if( !bFound )
		aHelpExistanceMap[ aExtensionPath ] = xHelpPackage.is();

	return xHelpPackage;
}

Reference< deployment::XPackage > ExtensionIteratorBase::implGetNextUserHelpPackage
	( Reference< deployment::XPackage >& o_xParentPackageBundle )
{
	Reference< deployment::XPackage > xHelpPackage;

	if( !m_bUserPackagesLoaded )
	{
        Reference< XExtensionManager > xExtensionManager = ExtensionManager::get(m_xContext);
		m_aUserPackagesSeq = xExtensionManager->getDeployedExtensions
			( rtl::OUString::createFromAscii("user"), Reference< task::XAbortChannel >(), Reference< ucb::XCommandEnvironment >() );
		m_bUserPackagesLoaded = true;
	}

	if( m_iUserPackage == m_aUserPackagesSeq.getLength() )
	{
		m_eState = SHARED_EXTENSIONS;		// Later: SHARED_MODULE
	}
	else
	{
		const Reference< deployment::XPackage >* pUserPackages = m_aUserPackagesSeq.getConstArray();
		Reference< deployment::XPackage > xPackage = pUserPackages[ m_iUserPackage++ ];
		VOS_ENSURE( xPackage.is(), "ExtensionIteratorBase::implGetNextUserHelpPackage(): Invalid package" );
		xHelpPackage = implGetHelpPackageFromPackage( xPackage, o_xParentPackageBundle );
	}

	return xHelpPackage;
}

Reference< deployment::XPackage > ExtensionIteratorBase::implGetNextSharedHelpPackage
	( Reference< deployment::XPackage >& o_xParentPackageBundle )
{
	Reference< deployment::XPackage > xHelpPackage;

	if( !m_bSharedPackagesLoaded )
	{
		Reference< XExtensionManager > xExtensionManager = ExtensionManager::get(m_xContext);
		m_aSharedPackagesSeq = xExtensionManager->getDeployedExtensions
			( rtl::OUString::createFromAscii("shared"), Reference< task::XAbortChannel >(), Reference< ucb::XCommandEnvironment >() );
		m_bSharedPackagesLoaded = true;
	}

	if( m_iSharedPackage == m_aSharedPackagesSeq.getLength() )
	{
		m_eState = BUNDLED_EXTENSIONS;
	}
	else
	{
		const Reference< deployment::XPackage >* pSharedPackages = m_aSharedPackagesSeq.getConstArray();
		Reference< deployment::XPackage > xPackage = pSharedPackages[ m_iSharedPackage++ ];
		VOS_ENSURE( xPackage.is(), "ExtensionIteratorBase::implGetNextSharedHelpPackage(): Invalid package" );
		xHelpPackage = implGetHelpPackageFromPackage( xPackage, o_xParentPackageBundle );
	}

	return xHelpPackage;
}

Reference< deployment::XPackage > ExtensionIteratorBase::implGetNextBundledHelpPackage
	( Reference< deployment::XPackage >& o_xParentPackageBundle )
{
	Reference< deployment::XPackage > xHelpPackage;

	if( !m_bBundledPackagesLoaded )
	{
		Reference< XExtensionManager > xExtensionManager = ExtensionManager::get(m_xContext);
		m_aBundledPackagesSeq = xExtensionManager->getDeployedExtensions
			( rtl::OUString::createFromAscii("bundled"), Reference< task::XAbortChannel >(), Reference< ucb::XCommandEnvironment >() );
		m_bBundledPackagesLoaded = true;
	}

	if( m_iBundledPackage == m_aBundledPackagesSeq.getLength() )
	{
		m_eState = END_REACHED;
	}
	else
	{
		const Reference< deployment::XPackage >* pBundledPackages =
            m_aBundledPackagesSeq.getConstArray();
		Reference< deployment::XPackage > xPackage = pBundledPackages[ m_iBundledPackage++ ];
		VOS_ENSURE( xPackage.is(), "ExtensionIteratorBase::implGetNextBundledHelpPackage(): Invalid package" );
		xHelpPackage = implGetHelpPackageFromPackage( xPackage, o_xParentPackageBundle );
	}

	return xHelpPackage;
}

rtl::OUString ExtensionIteratorBase::implGetFileFromPackage(
	const rtl::OUString& rFileExtension, Reference< deployment::XPackage > xPackage )
{
	// No extension -> search for pure language folder
	bool bLangFolderOnly = (rFileExtension.getLength() == 0);

	rtl::OUString aFile;
	rtl::OUString aLanguage = m_aLanguage;
	for( sal_Int32 iPass = 0 ; iPass < 2 ; ++iPass )
	{
		rtl::OUStringBuffer aStrBuf;
		aStrBuf.append( xPackage->getRegistrationDataURL().Value);
		aStrBuf.append( aSlash );
		aStrBuf.append( aLanguage );
		if( !bLangFolderOnly )
		{
			aStrBuf.append( aSlash );
			aStrBuf.append( aHelpFilesBaseName );
			aStrBuf.append( rFileExtension );
		}

		aFile = m_rDatabases.expandURL( aStrBuf.makeStringAndClear() );
		if( iPass == 0 )
		{
			if( m_xSFA->exists( aFile ) )
				break;

			::std::vector< ::rtl::OUString > av;
			implGetLanguageVectorFromPackage( av, xPackage );
			::std::vector< ::rtl::OUString >::const_iterator pFound = av.end();
			try
			{
				pFound = ::comphelper::Locale::getFallback( av, m_aLanguage );
			}
			catch( ::comphelper::Locale::MalFormedLocaleException& )
			{}
			if( pFound != av.end() )
				aLanguage = *pFound;
		}
	}
	return aFile;
}

inline bool isLetter( sal_Unicode c )
{
	bool bLetter = ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
	return bLetter;
}

void ExtensionIteratorBase::implGetLanguageVectorFromPackage( ::std::vector< ::rtl::OUString > &rv,
	com::sun::star::uno::Reference< com::sun::star::deployment::XPackage > xPackage )
{
	rv.clear();
	rtl::OUString aExtensionPath = xPackage->getURL();
	Sequence< rtl::OUString > aEntrySeq = m_xSFA->getFolderContents( aExtensionPath, true );

	const rtl::OUString* pSeq = aEntrySeq.getConstArray();
    sal_Int32 nCount = aEntrySeq.getLength();
	for( sal_Int32 i = 0 ; i < nCount ; ++i )
	{
		rtl::OUString aEntry = pSeq[i];
		if( m_xSFA->isFolder( aEntry ) )
		{
			sal_Int32 nLastSlash = aEntry.lastIndexOf( '/' );
			if( nLastSlash != -1 )
			{
				rtl::OUString aPureEntry = aEntry.copy( nLastSlash + 1 );

				// Check language sceme
				int nLen = aPureEntry.getLength();
				const sal_Unicode* pc = aPureEntry.getStr();
				bool bStartCanBeLanguage = ( nLen >= 2 && isLetter( pc[0] ) && isLetter( pc[1] ) );
				bool bIsLanguage = bStartCanBeLanguage &&
					( nLen == 2 || (nLen == 5 && pc[2] == '-' && isLetter( pc[3] ) && isLetter( pc[4] )) );
				if( bIsLanguage )
					rv.push_back( aPureEntry );
			}
		}
	}
}


//===================================================================
// class DataBaseIterator

helpdatafileproxy::Hdf* DataBaseIterator::nextHdf( rtl::OUString* o_pExtensionPath, rtl::OUString* o_pExtensionRegistryPath )
{
	helpdatafileproxy::Hdf* pRetHdf = NULL;

	while( !pRetHdf && m_eState != END_REACHED )
	{
		switch( m_eState )
		{
			case INITIAL_MODULE:
				pRetHdf = m_rDatabases.getHelpDataFile( m_aInitialModule, m_aLanguage, m_bHelpText );
				m_eState = USER_EXTENSIONS;		// Later: SHARED_MODULE
				break;

			// Later:
			//case SHARED_MODULE
				//...

			case USER_EXTENSIONS:
			{
				Reference< deployment::XPackage > xParentPackageBundle;
				Reference< deployment::XPackage > xHelpPackage = implGetNextUserHelpPackage( xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;
				pRetHdf = implGetHdfFromPackage( xHelpPackage, o_pExtensionPath, o_pExtensionRegistryPath );
				break;
			}

			case SHARED_EXTENSIONS:
			{
				Reference< deployment::XPackage > xParentPackageBundle;
				Reference< deployment::XPackage > xHelpPackage = implGetNextSharedHelpPackage( xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				pRetHdf = implGetHdfFromPackage( xHelpPackage, o_pExtensionPath, o_pExtensionRegistryPath );
				break;
			}

   			case BUNDLED_EXTENSIONS:
			{
				Reference< deployment::XPackage > xParentPackageBundle;
				Reference< deployment::XPackage > xHelpPackage = implGetNextBundledHelpPackage( xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				pRetHdf = implGetHdfFromPackage( xHelpPackage, o_pExtensionPath, o_pExtensionRegistryPath );
				break;
			}

			case END_REACHED:
				VOS_ENSURE( false, "DataBaseIterator::nextDb(): Invalid case END_REACHED" );
				break;
		}
	}

	return pRetHdf;
}

helpdatafileproxy::Hdf* DataBaseIterator::implGetHdfFromPackage( Reference< deployment::XPackage > xPackage,
            rtl::OUString* o_pExtensionPath, rtl::OUString* o_pExtensionRegistryPath )
{
    
    beans::Optional< ::rtl::OUString> optRegData;
    try
    {
        optRegData = xPackage->getRegistrationDataURL();
    }
    catch ( deployment::ExtensionRemovedException&)
    {
        return NULL;
    }

    helpdatafileproxy::Hdf* pRetHdf = NULL;
    if (optRegData.IsPresent && optRegData.Value.getLength() > 0)
    {
        rtl::OUString aRegDataUrl(optRegData.Value); 
        aRegDataUrl += aSlash;

        rtl::OUString aUsedLanguage = m_aLanguage;
        pRetHdf = m_rDatabases.getHelpDataFile(
            aHelpFilesBaseName, aUsedLanguage, m_bHelpText, &aRegDataUrl);

        // Language fallback
        if( !pRetHdf )
        {
            ::std::vector< ::rtl::OUString > av;
            implGetLanguageVectorFromPackage( av, xPackage );
            ::std::vector< ::rtl::OUString >::const_iterator pFound = av.end();
            try
            {
                pFound = ::comphelper::Locale::getFallback( av, m_aLanguage );
            }
            catch( ::comphelper::Locale::MalFormedLocaleException& )
            {}
            if( pFound != av.end() )
            {
                aUsedLanguage = *pFound;
                pRetHdf = m_rDatabases.getHelpDataFile(
                    aHelpFilesBaseName, aUsedLanguage, m_bHelpText, &aRegDataUrl);
            }
        }

        if( o_pExtensionPath )
            *o_pExtensionPath = aRegDataUrl + aUsedLanguage;

        if( o_pExtensionRegistryPath )
            *o_pExtensionRegistryPath = xPackage->getURL() + aSlash + aUsedLanguage;
    }
    
	return pRetHdf;
}


//===================================================================
// class KeyDataBaseFileIterator

//returns a file URL
rtl::OUString KeyDataBaseFileIterator::nextDbFile( bool& o_rbExtension )
{
	rtl::OUString aRetFile;

	while( !aRetFile.getLength() && m_eState != END_REACHED )
	{
		switch( m_eState )
		{
			case INITIAL_MODULE:
				aRetFile = 
                    m_rDatabases.getInstallPathAsURL() +
					m_rDatabases.processLang( m_aLanguage ) + aSlash + m_aInitialModule +
					rtl::OUString::createFromAscii( ".key" );

				o_rbExtension = false;

				m_eState = USER_EXTENSIONS;		// Later: SHARED_MODULE
				break;

			// Later:
			//case SHARED_MODULE
				//...

			case USER_EXTENSIONS:
			{
				Reference< deployment::XPackage > xParentPackageBundle;
				Reference< deployment::XPackage > xHelpPackage = implGetNextUserHelpPackage( xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				aRetFile = implGetDbFileFromPackage( xHelpPackage );
				o_rbExtension = true;
				break;
			}

			case SHARED_EXTENSIONS:
			{
				Reference< deployment::XPackage > xParentPackageBundle;
				Reference< deployment::XPackage > xHelpPackage = implGetNextSharedHelpPackage( xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				aRetFile = implGetDbFileFromPackage( xHelpPackage );
				o_rbExtension = true;
				break;
			}

            case BUNDLED_EXTENSIONS:
			{
				Reference< deployment::XPackage > xParentPackageBundle;
				Reference< deployment::XPackage > xHelpPackage = implGetNextBundledHelpPackage( xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				aRetFile = implGetDbFileFromPackage( xHelpPackage );
				o_rbExtension = true;
				break;
			}

            case END_REACHED:
				VOS_ENSURE( false, "DataBaseIterator::nextDbFile(): Invalid case END_REACHED" );
				break;
		}
	}

	return aRetFile;
}

//Returns a file URL, that does not contain macros
rtl::OUString KeyDataBaseFileIterator::implGetDbFileFromPackage
	( Reference< deployment::XPackage > xPackage )
{
	rtl::OUString aExpandedURL =
		implGetFileFromPackage( rtl::OUString::createFromAscii( ".key" ), xPackage );

    return aExpandedURL;
}


//===================================================================
// class JarFileIterator

Reference< XHierarchicalNameAccess > JarFileIterator::nextJarFile
	( Reference< deployment::XPackage >& o_xParentPackageBundle,
        rtl::OUString* o_pExtensionPath, rtl::OUString* o_pExtensionRegistryPath )
{
	Reference< XHierarchicalNameAccess > xNA;

	while( !xNA.is() && m_eState != END_REACHED )
	{
		switch( m_eState )
		{
			case INITIAL_MODULE:
				xNA = m_rDatabases.jarFile( m_aInitialModule, m_aLanguage );
				m_eState = USER_EXTENSIONS;		// Later: SHARED_MODULE
				break;

			// Later:
			//case SHARED_MODULE
				//...

			case USER_EXTENSIONS:
			{
				Reference< deployment::XPackage > xHelpPackage = implGetNextUserHelpPackage( o_xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				xNA = implGetJarFromPackage( xHelpPackage, o_pExtensionPath, o_pExtensionRegistryPath );
				break;
			}

			case SHARED_EXTENSIONS:
			{
				Reference< deployment::XPackage > xHelpPackage = implGetNextSharedHelpPackage( o_xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				xNA = implGetJarFromPackage( xHelpPackage, o_pExtensionPath, o_pExtensionRegistryPath );
				break;
			}

			case BUNDLED_EXTENSIONS:
			{
				Reference< deployment::XPackage > xHelpPackage = implGetNextBundledHelpPackage( o_xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				xNA = implGetJarFromPackage( xHelpPackage, o_pExtensionPath, o_pExtensionRegistryPath );
				break;
			}

            case END_REACHED:
				VOS_ENSURE( false, "JarFileIterator::nextJarFile(): Invalid case END_REACHED" );
				break;
		}
	}

	return xNA;
}

Reference< XHierarchicalNameAccess > JarFileIterator::implGetJarFromPackage
( Reference< deployment::XPackage > xPackage, rtl::OUString* o_pExtensionPath, rtl::OUString* o_pExtensionRegistryPath )
{
	Reference< XHierarchicalNameAccess > xNA;

	rtl::OUString zipFile =
		implGetFileFromPackage( rtl::OUString::createFromAscii( ".jar" ), xPackage );

	try
	{
        Sequence< Any > aArguments( 2 );
        aArguments[ 0 ] <<= zipFile;

        // let ZipPackage be used ( no manifest.xml is required )
        beans::NamedValue aArg;
        aArg.Name = ::rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "StorageFormat" ) );
        aArg.Value <<= ZIP_STORAGE_FORMAT_STRING;
        aArguments[ 1 ] <<= aArg;

		Reference< XMultiComponentFactory >xSMgr( m_xContext->getServiceManager(), UNO_QUERY );
		Reference< XInterface > xIfc
			= xSMgr->createInstanceWithArgumentsAndContext(
				rtl::OUString::createFromAscii(
					"com.sun.star.packages.comp.ZipPackage" ),
				aArguments, m_xContext );
		
		if ( xIfc.is() )
		{
			xNA = Reference< XHierarchicalNameAccess >( xIfc, UNO_QUERY );
			
			VOS_ENSURE( xNA.is(),
				"JarFileIterator::implGetJarFromPackage() - "
				"Got no hierarchical name access!" );
		}            
	}
	catch ( RuntimeException & )
	{}
	catch ( Exception & )
	{}

	if( xNA.is() && o_pExtensionPath != NULL )
	{
		// Extract path including language from file name
		sal_Int32 nLastSlash = zipFile.lastIndexOf( '/' );
		if( nLastSlash != -1 )
			*o_pExtensionPath = zipFile.copy( 0, nLastSlash );
        
        if( o_pExtensionRegistryPath != NULL )
        {
            rtl::OUString& rPath = *o_pExtensionPath;
            sal_Int32 nLastSlashInPath = rPath.lastIndexOf( '/', rPath.getLength() - 1 );

            *o_pExtensionRegistryPath = xPackage->getURL();
            *o_pExtensionRegistryPath += rPath.copy( nLastSlashInPath);
        }
	}
	
	return xNA;
}


//===================================================================
// class IndexFolderIterator

rtl::OUString IndexFolderIterator::nextIndexFolder( bool& o_rbExtension, bool& o_rbTemporary )
{
	rtl::OUString aIndexFolder;

	while( !aIndexFolder.getLength() && m_eState != END_REACHED )
	{
		switch( m_eState )
		{
			case INITIAL_MODULE:
				aIndexFolder = 
					m_rDatabases.getInstallPathAsURL() +
					m_rDatabases.processLang( m_aLanguage ) + aSlash + m_aInitialModule +
					rtl::OUString::createFromAscii( ".idxl" );

				o_rbTemporary = false;
				o_rbExtension = false;

				m_eState = USER_EXTENSIONS;		// Later: SHARED_MODULE
				break;

			// Later:
			//case SHARED_MODULE
				//...

			case USER_EXTENSIONS:
			{
				Reference< deployment::XPackage > xParentPackageBundle;
				Reference< deployment::XPackage > xHelpPackage = implGetNextUserHelpPackage( xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				aIndexFolder = implGetIndexFolderFromPackage( o_rbTemporary, xHelpPackage );
				o_rbExtension = true;
				break;
			}

			case SHARED_EXTENSIONS:
			{
				Reference< deployment::XPackage > xParentPackageBundle;
				Reference< deployment::XPackage > xHelpPackage = implGetNextSharedHelpPackage( xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				aIndexFolder = implGetIndexFolderFromPackage( o_rbTemporary, xHelpPackage );
				o_rbExtension = true;
				break;
			}
            
			case BUNDLED_EXTENSIONS:
			{
				Reference< deployment::XPackage > xParentPackageBundle;
				Reference< deployment::XPackage > xHelpPackage = implGetNextBundledHelpPackage( xParentPackageBundle );
				if( !xHelpPackage.is() )
					break;

				aIndexFolder = implGetIndexFolderFromPackage( o_rbTemporary, xHelpPackage );
				o_rbExtension = true;
				break;
			}

            case END_REACHED:
				VOS_ENSURE( false, "IndexFolderIterator::nextIndexFolder(): Invalid case END_REACHED" );
				break;
		}
	}

	return aIndexFolder;
}

rtl::OUString IndexFolderIterator::implGetIndexFolderFromPackage( bool& o_rbTemporary, Reference< deployment::XPackage > xPackage )
{
	rtl::OUString aIndexFolder =
		implGetFileFromPackage( rtl::OUString::createFromAscii( ".idxl" ), xPackage );

	o_rbTemporary = false;
	if( !m_xSFA->isFolder( aIndexFolder ) )
	{
		// i98680: Missing index? Try to generate now
		rtl::OUString aLangURL = implGetFileFromPackage( rtl::OUString(), xPackage );
		if( m_xSFA->isFolder( aLangURL ) )
		{
			// Test write access (shared extension may be read only)
			bool bIsWriteAccess = false;
			try
			{
				rtl::OUString aCreateTestFolder = aLangURL + rtl::OUString::createFromAscii( "CreateTestFolder" );
				m_xSFA->createFolder( aCreateTestFolder );
				if( m_xSFA->isFolder( aCreateTestFolder  ) )
					bIsWriteAccess = true;

				m_xSFA->kill( aCreateTestFolder );
			}
			catch (Exception &)
			{}

			// TEST
			//bIsWriteAccess = false;

			Reference< script::XInvocation > xInvocation;
			Reference< XMultiComponentFactory >xSMgr( m_xContext->getServiceManager(), UNO_QUERY );
			try
			{
				xInvocation = Reference< script::XInvocation >( 
					m_xContext->getServiceManager()->createInstanceWithContext( rtl::OUString::createFromAscii(
					"com.sun.star.help.HelpIndexer" ), m_xContext ) , UNO_QUERY );

				if( xInvocation.is() )
				{
					Sequence<uno::Any> aParamsSeq( bIsWriteAccess ? 6 : 8 );

					aParamsSeq[0] = uno::makeAny( rtl::OUString::createFromAscii( "-lang" ) );

					rtl::OUString aLang;
					sal_Int32 nLastSlash = aLangURL.lastIndexOf( '/' );
					if( nLastSlash != -1 )
						aLang = aLangURL.copy( nLastSlash + 1 );
					else
						aLang = rtl::OUString::createFromAscii( "en" );
					aParamsSeq[1] = uno::makeAny( aLang );

					aParamsSeq[2] = uno::makeAny( rtl::OUString::createFromAscii( "-mod" ) );
					aParamsSeq[3] = uno::makeAny( rtl::OUString::createFromAscii( "help" ) );

					rtl::OUString aZipDir = aLangURL;
					if( !bIsWriteAccess )
					{
						rtl::OUString aTempFileURL;
						::osl::FileBase::RC eErr = ::osl::File::createTempFile( 0, 0, &aTempFileURL );
						if( eErr == ::osl::FileBase::E_None )
						{
							rtl::OUString aTempDirURL = aTempFileURL;
							try
							{
								m_xSFA->kill( aTempDirURL );
							}
							catch (Exception &)
							{}
							m_xSFA->createFolder( aTempDirURL );

							aZipDir = aTempDirURL;
							o_rbTemporary = true;
						}
					}

					aParamsSeq[4] = uno::makeAny( rtl::OUString::createFromAscii( "-zipdir" ) );
					rtl::OUString aSystemPath;
					osl::FileBase::getSystemPathFromFileURL( aZipDir, aSystemPath );
					aParamsSeq[5] = uno::makeAny( aSystemPath );

					if( !bIsWriteAccess )
					{
						aParamsSeq[6] = uno::makeAny( rtl::OUString::createFromAscii( "-srcdir" ) );
						rtl::OUString aSrcDirVal;
						osl::FileBase::getSystemPathFromFileURL( aLangURL, aSrcDirVal );
						aParamsSeq[7] = uno::makeAny( aSrcDirVal );
					}

					Sequence< sal_Int16 > aOutParamIndex;
					Sequence< uno::Any > aOutParam;
					uno::Any aRet = xInvocation->invoke( rtl::OUString::createFromAscii( "createIndex" ),
						aParamsSeq, aOutParamIndex, aOutParam );

					if( bIsWriteAccess )
						aIndexFolder = implGetFileFromPackage( rtl::OUString::createFromAscii( ".idxl" ), xPackage );
					else
						aIndexFolder = aZipDir + rtl::OUString::createFromAscii( "/help.idxl" );
				}
			}
			catch (Exception &)
			{}
		}
	}

	return aIndexFolder;
}

void IndexFolderIterator::deleteTempIndexFolder( const rtl::OUString& aIndexFolder )
{
	sal_Int32 nLastSlash = aIndexFolder.lastIndexOf( '/' );
	if( nLastSlash != -1 )
	{
		rtl::OUString aTmpFolder = aIndexFolder.copy( 0, nLastSlash );
		try
		{
			m_xSFA->kill( aTmpFolder );
		}
		catch (Exception &)
		{}
	}
}
