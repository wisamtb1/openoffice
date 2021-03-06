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
#include "precompiled_stoc.hxx"

#include <hash_map>
#include <osl/mutex.hxx>
#include <osl/diagnose.h>
#include <uno/dispatcher.h>
#include <uno/mapping.hxx>
#include <cppuhelper/queryinterface.hxx>
#include <cppuhelper/weak.hxx>
#include <cppuhelper/factory.hxx>
#include <cppuhelper/component.hxx>
#include <cppuhelper/implbase2.hxx>
#ifndef _CPPUHELPER_IMPLEMENTATIONENTRY_HXX_
#include <cppuhelper/implementationentry.hxx>
#endif

#include <com/sun/star/uno/XNamingService.hpp>
#include <com/sun/star/lang/XServiceInfo.hpp>

using namespace cppu;
using namespace rtl;
using namespace osl;
using namespace std;

using namespace com::sun::star::uno;
using namespace com::sun::star::lang;
using namespace com::sun::star::registry;

#define SERVICENAME "com.sun.star.uno.NamingService"
#define IMPLNAME	"com.sun.star.comp.stoc.NamingService"

namespace stoc_namingservice
{
static rtl_StandardModuleCount g_moduleCount = MODULE_COUNT_INIT;

static Sequence< OUString > ns_getSupportedServiceNames()
{
	static Sequence < OUString > *pNames = 0;
	if( ! pNames )
	{
		MutexGuard guard( Mutex::getGlobalMutex() );
		if( !pNames )
		{
			static Sequence< OUString > seqNames(1);
			seqNames.getArray()[0] = OUString(RTL_CONSTASCII_USTRINGPARAM(SERVICENAME));
			pNames = &seqNames;
		}
	}
	return *pNames;
}

static OUString ns_getImplementationName()
{
	static OUString *pImplName = 0;
	if( ! pImplName )
	{
		MutexGuard guard( Mutex::getGlobalMutex() );
		if( ! pImplName )
		{
			static OUString implName( RTL_CONSTASCII_USTRINGPARAM( IMPLNAME ) );
			pImplName = &implName;
		}
	}
	return *pImplName;
}

struct equalOWString_Impl
{
  sal_Bool operator()(const OUString & s1, const OUString & s2) const
		{ return s1 == s2; }
};

struct hashOWString_Impl
{
	size_t operator()(const OUString & rName) const
		{ return rName.hashCode(); }
};

typedef hash_map
<
	OUString,
	Reference<XInterface >,
	hashOWString_Impl,
	equalOWString_Impl
> HashMap_OWString_Interface;

//==================================================================================================
class NamingService_Impl
	: public WeakImplHelper2 < XServiceInfo, XNamingService >
{
	Mutex								aMutex;
	HashMap_OWString_Interface			aMap;
public:
	NamingService_Impl();
	~NamingService_Impl();

    // XServiceInfo
    virtual OUString SAL_CALL getImplementationName()
	throw(::com::sun::star::uno::RuntimeException);
    virtual sal_Bool SAL_CALL supportsService( const OUString& ServiceName )
	throw(::com::sun::star::uno::RuntimeException);
    virtual Sequence< OUString > SAL_CALL getSupportedServiceNames()
	throw(::com::sun::star::uno::RuntimeException);
    static Sequence< OUString > SAL_CALL getSupportedServiceNames_Static()
	{
		OUString aStr( OUString::createFromAscii( SERVICENAME ) );
		return Sequence< OUString >( &aStr, 1 );
	}

    virtual ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface > SAL_CALL getRegisteredObject( const ::rtl::OUString& Name ) throw(Exception, RuntimeException);
    virtual void SAL_CALL registerObject( const ::rtl::OUString& Name, const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XInterface >& Object ) throw(Exception, RuntimeException);
    virtual void SAL_CALL revokeObject( const ::rtl::OUString& Name ) throw(Exception, RuntimeException);
};

//==================================================================================================
static Reference<XInterface> SAL_CALL NamingService_Impl_create( const Reference<XComponentContext> & )
{
	return *new NamingService_Impl();
}

//==================================================================================================
NamingService_Impl::NamingService_Impl()
{
	g_moduleCount.modCnt.acquire( &g_moduleCount.modCnt );
}

//==================================================================================================
NamingService_Impl::~NamingService_Impl()
{
	g_moduleCount.modCnt.release( &g_moduleCount.modCnt );
}

// XServiceInfo
OUString NamingService_Impl::getImplementationName()
	throw(::com::sun::star::uno::RuntimeException)
{
	return ns_getImplementationName();
}

// XServiceInfo
sal_Bool NamingService_Impl::supportsService( const OUString & rServiceName )
	throw(::com::sun::star::uno::RuntimeException)
{
	const Sequence< OUString > & rSNL = getSupportedServiceNames();
	const OUString * pArray = rSNL.getConstArray();
	for ( sal_Int32 nPos = rSNL.getLength(); nPos--; )
	{
		if (pArray[nPos] == rServiceName)
			return sal_True;
	}
	return sal_False;
}

// XServiceInfo
Sequence< OUString > NamingService_Impl::getSupportedServiceNames()
	throw(::com::sun::star::uno::RuntimeException)
{
	return ns_getSupportedServiceNames();
}

// XServiceInfo
Reference< XInterface > NamingService_Impl::getRegisteredObject( const OUString& Name ) throw(Exception, RuntimeException)
{
	Guard< Mutex > aGuard( aMutex );
	Reference< XInterface > xRet;
	HashMap_OWString_Interface::iterator aIt = aMap.find( Name );
	if( aIt != aMap.end() )
		xRet = (*aIt).second;
	return xRet;
}

// XServiceInfo
void NamingService_Impl::registerObject( const OUString& Name, const Reference< XInterface >& Object ) throw(Exception, RuntimeException)
{
	Guard< Mutex > aGuard( aMutex );
	aMap[ Name ] = Object;
}

// XServiceInfo
void NamingService_Impl::revokeObject( const OUString& Name ) throw(Exception, RuntimeException)
{
	Guard< Mutex > aGuard( aMutex );
	aMap.erase( Name );
}

}

using namespace stoc_namingservice;
static struct ImplementationEntry g_entries[] =
{
	{
		NamingService_Impl_create, ns_getImplementationName,
		ns_getSupportedServiceNames, createSingleComponentFactory,
		&g_moduleCount.modCnt , 0
	},
	{ 0, 0, 0, 0, 0, 0 }
};

extern "C"
{
sal_Bool SAL_CALL component_canUnload( TimeValue *pTime )
{
	return g_moduleCount.canUnload( &g_moduleCount , pTime );
}

//==================================================================================================
void SAL_CALL component_getImplementationEnvironment(
	const sal_Char ** ppEnvTypeName, uno_Environment ** )
{
	*ppEnvTypeName = CPPU_CURRENT_LANGUAGE_BINDING_NAME;
}
//==================================================================================================
void * SAL_CALL component_getFactory(
	const sal_Char * pImplName, void * pServiceManager, void * pRegistryKey )
{
	return component_getFactoryHelper( pImplName, pServiceManager, pRegistryKey , g_entries );
}
}
