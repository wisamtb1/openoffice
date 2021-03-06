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



#include <cppuhelper/implementationentry.hxx>
#include <WriterFilterDetection.hxx>
#include <comphelper/storagehelper.hxx>
#include <com/sun/star/io/XInputStream.hpp>
#include <sot/storage.hxx>
//#ifndef _SFXDOCFILE_HXX //todo: remove sfx2!
//#include <sfx2/docfile.hxx>
//#endif
#include <unotools/ucbstreamhelper.hxx>

using namespace ::rtl;
using namespace ::cppu;
using namespace ::com::sun::star;

/*-- 22.02.2007 12:17:53---------------------------------------------------

  -----------------------------------------------------------------------*/
WriterFilterDetection::WriterFilterDetection(
    const uno::Reference< uno::XComponentContext >& rxContext) :
    m_xContext( rxContext )
{
}
/*-- 22.02.2007 12:17:53---------------------------------------------------

  -----------------------------------------------------------------------*/
WriterFilterDetection::~WriterFilterDetection()
{
}
/*-- 22.02.2007 12:11:38---------------------------------------------------

  -----------------------------------------------------------------------*/
OUString WriterFilterDetection_getImplementationName () throw (uno::RuntimeException)
{
   return OUString ( RTL_CONSTASCII_USTRINGPARAM ( "com.sun.star.comp.Writer.WriterFilterDetector" ) );
}

#define SERVICE_NAME1 "com.sun.star.document.ExtendedTypeDetection"
/*-- 22.02.2007 12:11:38---------------------------------------------------

  -----------------------------------------------------------------------*/
OUString WriterFilterDetection::detect( uno::Sequence< beans::PropertyValue >& rDescriptor )
   throw( uno::RuntimeException )
{
    OUString sTypeName;
    bool bWord = false;
    sal_Int32 nPropertyCount = rDescriptor.getLength();
    const beans::PropertyValue* pValues = rDescriptor.getConstArray();
    rtl::OUString sURL;
    uno::Reference < io::XStream > xStream;
    uno::Reference < io::XInputStream > xInputStream;
    for( sal_Int32 nProperty = 0; nProperty < nPropertyCount; ++nProperty )
    {
        if( pValues[nProperty].Name == OUString(RTL_CONSTASCII_USTRINGPARAM("TypeName")) )
            rDescriptor[nProperty].Value >>= sTypeName;
        else if( pValues[nProperty].Name.equalsAsciiL(RTL_CONSTASCII_STRINGPARAM ( "URL" )) )
            pValues[nProperty].Value >>= sURL;
        else if( pValues[nProperty].Name.equalsAsciiL(RTL_CONSTASCII_STRINGPARAM ( "Stream" )) )
            pValues[nProperty].Value >>= xStream;
        else if( pValues[nProperty].Name.equalsAsciiL(RTL_CONSTASCII_STRINGPARAM ( "InputStream" )) )
            pValues[nProperty].Value >>= xInputStream;
    }
    bool bBinary = sTypeName.equalsAsciiL ( RTL_CONSTASCII_STRINGPARAM ( "writer_MS_Word_97" )) ||
                   sTypeName.equalsAsciiL ( RTL_CONSTASCII_STRINGPARAM ( "writer_MS_Word_97_Vorlage" ));

    try
    {
        if(bBinary)
        {
            SvStream* pStream = ::utl::UcbStreamHelper::CreateStream( xInputStream );
            if ( pStream && SotStorage::IsStorageFile(pStream) )

            {
                SotStorageRef xStg = new SotStorage( pStream, sal_False );

                bool bTable2 = xStg->IsContained( rtl::OUString::createFromAscii("1Table" ));
                SotStorageStreamRef xRef =

                    xStg->OpenSotStream(rtl::OUString::createFromAscii("WordDocument"),

                            STREAM_STD_READ | STREAM_NOCREATE );

                if(bTable2 && xStg.Is())
                {
                    xRef->Seek(2);
                    sal_Int16 nWord;
                    *xRef >> nWord;
                    //version detection
                    bWord = nWord >= 0x6a && nWord <= 0xc1;
                }
            }
        }
        else
        {
            uno::Reference< embed::XStorage > xDocStorage;
            if( sURL.equalsAscii( "private:stream" ) )
                xDocStorage = comphelper::OStorageHelper::GetStorageFromInputStream( xInputStream );
            else
                xDocStorage = comphelper::OStorageHelper::GetStorageFromURL(
                                            sURL, embed::ElementModes::READ );
            if( xDocStorage.is() )
            {
                uno::Sequence< ::rtl::OUString > aNames = xDocStorage->getElementNames();
                const ::rtl::OUString* pNames = aNames.getConstArray();
                for(sal_Int32 nName = 0; nName < aNames.getLength(); ++nName)
                {
                    if(pNames[nName].equalsAsciiL(RTL_CONSTASCII_STRINGPARAM ( "word" )))
                    {
                        bWord = true;
                        if( !sTypeName.getLength() )
                            sTypeName = ::rtl::OUString( 
                                    RTL_CONSTASCII_STRINGPARAM( "writer_MS_Word_2007" ), RTL_TEXTENCODING_ASCII_US);
                        break;
                    }
                }
            }
        }
    }
    catch(const uno::Exception&)
    {
        OSL_ASSERT("exception while opening storage");
    }
    if( !bWord )
        sTypeName = ::rtl::OUString();
   return sTypeName;
}
/*-- 22.02.2007 12:11:38---------------------------------------------------

  -----------------------------------------------------------------------*/
sal_Bool WriterFilterDetection_supportsService( const OUString& ServiceName ) throw (uno::RuntimeException)
{
   return (ServiceName.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM ( SERVICE_NAME1 ) ) );
}
/*-- 22.02.2007 12:11:38---------------------------------------------------

  -----------------------------------------------------------------------*/
uno::Sequence< OUString > WriterFilterDetection_getSupportedServiceNames(  ) throw (uno::RuntimeException)
{
   uno::Sequence < OUString > aRet(1);
   OUString* pArray = aRet.getArray();
   pArray[0] =  OUString ( RTL_CONSTASCII_USTRINGPARAM ( SERVICE_NAME1 ) );
   return aRet;
}
#undef SERVICE_NAME1
/*-- 22.02.2007 12:11:38---------------------------------------------------

  -----------------------------------------------------------------------*/
uno::Reference< uno::XInterface > WriterFilterDetection_createInstance( const uno::Reference< uno::XComponentContext >& xContext)
                throw( uno::Exception )
{
   return (cppu::OWeakObject*) new WriterFilterDetection( xContext );
}
/*-- 22.02.2007 12:11:38---------------------------------------------------

  -----------------------------------------------------------------------*/
OUString WriterFilterDetection::getImplementationName(  ) throw (uno::RuntimeException)
{
   return WriterFilterDetection_getImplementationName();
}
/*-- 22.02.2007 12:11:38---------------------------------------------------

  -----------------------------------------------------------------------*/
sal_Bool WriterFilterDetection::supportsService( const OUString& rServiceName ) throw (uno::RuntimeException)
{
    return WriterFilterDetection_supportsService( rServiceName );
}
/*-- 22.02.2007 12:11:38---------------------------------------------------

  -----------------------------------------------------------------------*/
uno::Sequence< OUString > WriterFilterDetection::getSupportedServiceNames(  ) throw (uno::RuntimeException)
{
    return WriterFilterDetection_getSupportedServiceNames();
}

