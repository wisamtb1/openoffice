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
#include "precompiled_rptui.hxx"

#include <ReportControllerObserver.hxx>
#include <ReportController.hxx>
#include <svl/smplhint.hxx>
#include <vos/mutex.hxx>
#include <vcl/svapp.hxx>
#include <com/sun/star/report/XFormattedField.hpp>
#include <com/sun/star/awt/FontSlant.hpp>
#include <FormattedFieldBeautifier.hxx>

#include <svx/unopage.hxx>

// DBG_*
#include <tools/debug.hxx>
// DBG_UNHANDLED_EXCEPTION
#include <tools/diagnose_ex.h>

namespace rptui
{

	using namespace ::com::sun::star;

    // const OReportController *& m_pReportController;

DECLARE_STL_USTRINGACCESS_MAP(bool, AllProperties);
DECLARE_STL_STDKEY_MAP(uno::Reference< beans::XPropertySet >, AllProperties, PropertySetInfoCache);

class OXReportControllerObserverImpl
{
    OXReportControllerObserverImpl(OXReportControllerObserverImpl&);
    void operator =(OXReportControllerObserverImpl&);
public:
    const OReportController&                            m_rReportController;
	::std::vector< uno::Reference< container::XChild> > m_aSections;
    ::osl::Mutex                                        m_aMutex;
    oslInterlockedCount                                 m_nLocks;
	sal_Bool                                            m_bReadOnly;

    OXReportControllerObserverImpl(const OReportController& _rController);
    ~OXReportControllerObserverImpl();
};

// -----------------------------------------------------------------------------

    OXReportControllerObserverImpl::OXReportControllerObserverImpl(const OReportController& _rController)
            :m_rReportController(_rController)
            ,m_nLocks(0)
            ,m_bReadOnly(sal_False)
    {
    }
    
    OXReportControllerObserverImpl::~OXReportControllerObserverImpl()
    {
    }

    // -----------------------------------------------------------------------------
    // -----------------------------------------------------------------------------
    // -----------------------------------------------------------------------------

    DBG_NAME(rpt_OXReportControllerObserver)

    OXReportControllerObserver::OXReportControllerObserver(const OReportController& _rController)
            :m_pImpl(new OXReportControllerObserverImpl(_rController) )
            ,m_aFormattedFieldBeautifier(_rController)
            ,m_aFixedTextColor(_rController)
    {
        DBG_CTOR( rpt_OXReportControllerObserver,NULL);

        Application::AddEventListener(LINK( this, OXReportControllerObserver, SettingsChanged ) );
    }

    OXReportControllerObserver::~OXReportControllerObserver()
    {
        DBG_CTOR( rpt_OXReportControllerObserver,NULL);
        Application::RemoveEventListener(LINK( this, OXReportControllerObserver, SettingsChanged ) );
    }

    // -----------------------------------------------------------------------------
	IMPL_LINK(OXReportControllerObserver, SettingsChanged, VclWindowEvent*, _pEvt)
	{
		if ( _pEvt )
        {
            sal_Int32 nEvent = _pEvt->GetId();
            /*
              // just for debug
            if (nEvent == VCLEVENT_WINDOW_CHILDCREATED ||
                nEvent == VCLEVENT_WINDOW_PAINT ||
                nEvent == VCLEVENT_WINDOW_MOVE ||
                nEvent == VCLEVENT_WINDOW_RESIZE ||
                nEvent == VCLEVENT_WINDOW_SHOW ||
                nEvent == VCLEVENT_WINDOW_MOUSEMOVE ||
                nEvent == VCLEVENT_WINDOW_FRAMETITLECHANGED || 
                nEvent == VCLEVENT_WINDOW_HIDE ||
                nEvent == VCLEVENT_EDIT_MODIFY ||
                nEvent == VCLEVENT_SCROLLBAR_ENDSCROLL ||
                nEvent == VCLEVENT_EDIT_SELECTIONCHANGED ||
                nEvent == VCLEVENT_TABPAGE_INSERTED ||
                nEvent == VCLEVENT_TABPAGE_REMOVED ||
                nEvent == VCLEVENT_TOOLBOX_FORMATCHANGED ||
                nEvent == VCLEVENT_TOOLBOX_ITEMADDED ||
                nEvent == VCLEVENT_TOOLBOX_ALLITEMCHANGED ||
                nEvent == VCLEVENT_MENUBARADDED ||
                nEvent == 1
                )
            {
                return 0L;
            }
            */
            
            if (nEvent == VCLEVENT_APPLICATION_DATACHANGED )
            {
                DataChangedEvent* pData = reinterpret_cast<DataChangedEvent*>(_pEvt->GetData());
                if ( pData && ((( pData->GetType() == DATACHANGED_SETTINGS	)	||
                                ( pData->GetType() == DATACHANGED_DISPLAY	))	&&
                               ( pData->GetFlags() & SETTINGS_STYLE		)))
                {
                    OEnvLock aLock(*this);
                    
                    // sal_uInt32 nCount = m_pImpl->m_aSections.size();
                    
                    // send all Section Objects a 'tingle'
                    // maybe they need a change in format, color, etc
                    ::std::vector< uno::Reference< container::XChild > >::const_iterator aIter = m_pImpl->m_aSections.begin();
                    ::std::vector< uno::Reference< container::XChild > >::const_iterator aEnd = m_pImpl->m_aSections.end();
                    for (;aIter != aEnd; aIter++)
                    {
                        const uno::Reference<container::XChild> xChild (*aIter);
                        if (xChild.is())
                        {
                            uno::Reference<report::XSection> xSection(xChild, uno::UNO_QUERY);
                            if (xSection.is())
                            {    
                                const sal_Int32 nCount = xSection->getCount();
                                for (sal_Int32 i = 0; i < nCount; ++i)
                                {
                                    const uno::Any aObj = xSection->getByIndex(i);
                                    uno::Reference < report::XReportComponent > xReportComponent(aObj, uno::UNO_QUERY);
                                    if (xReportComponent.is())
                                    {
                                        m_aFormattedFieldBeautifier.handle(xReportComponent);
                                        m_aFixedTextColor.handle(xReportComponent);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        return 0L;
    }

    // XEventListener
    void SAL_CALL OXReportControllerObserver::disposing(const lang::EventObject& e) throw( uno::RuntimeException )
    {
        (void) e;
	    // check if it's an object we have cached informations about
        uno::Reference< beans::XPropertySet > xSourceSet(e.Source, uno::UNO_QUERY);
        if ( xSourceSet.is() )
        {
            uno::Reference< report::XSection> xSection(xSourceSet,uno::UNO_QUERY);
            if ( xSection.is() )
                RemoveSection(xSection);
            else
                RemoveElement(xSourceSet);
	    }
    }
    
    void OXReportControllerObserver::Clear()
    {
        OEnvLock aLock(*this);
        // sal_uInt32 nDebugValue = m_pImpl->m_aSections.size();
        m_pImpl->m_aSections.clear();
    }
    
    // XPropertyChangeListener
    void SAL_CALL OXReportControllerObserver::propertyChange(const beans::PropertyChangeEvent& _rEvent) throw(uno::RuntimeException)
    {
        (void) _rEvent;
        ::osl::ClearableMutexGuard aGuard( m_pImpl->m_aMutex );

    	if ( IsLocked() )
            return;

        m_aFormattedFieldBeautifier.notifyPropertyChange(_rEvent);
        m_aFixedTextColor.notifyPropertyChange(_rEvent);        
    }
    
// -----------------------------------------------------------------------------
void OXReportControllerObserver::Lock() 
{ 
    OSL_ENSURE(m_refCount,"Illegal call to dead object!");
    osl_incrementInterlockedCount( &m_pImpl->m_nLocks ); 
}
void OXReportControllerObserver::UnLock() 
{ 
    OSL_ENSURE(m_refCount,"Illegal call to dead object!");

    osl_decrementInterlockedCount( &m_pImpl->m_nLocks ); 
}
sal_Bool OXReportControllerObserver::IsLocked() const { return m_pImpl->m_nLocks != 0; }

//------------------------------------------------------------------------------
void OXReportControllerObserver::AddSection(const uno::Reference< report::XSection > & _xSection)
{
    OEnvLock aLock(*this);
    try
    {
	    uno::Reference<container::XChild> xChild = _xSection.get();
	    m_pImpl->m_aSections.push_back(xChild);
	    uno::Reference< uno::XInterface >  xInt(_xSection);
	    AddElement(xInt);
    }
    catch(const uno::Exception&)
    {
        DBG_UNHANDLED_EXCEPTION();
    }
}

//------------------------------------------------------------------------------
void OXReportControllerObserver::RemoveSection(const uno::Reference< report::XSection > & _xSection)
{
	OEnvLock aLock(*this);
    try
    {
	    uno::Reference<container::XChild> xChild(_xSection.get());
	    m_pImpl->m_aSections.erase(::std::remove(m_pImpl->m_aSections.begin(),m_pImpl->m_aSections.end(),
		    xChild), m_pImpl->m_aSections.end());
	    uno::Reference< uno::XInterface >  xInt(_xSection);
	    RemoveElement(xInt);
    }
    catch(uno::Exception&)
    {
        DBG_UNHANDLED_EXCEPTION();
    }
}

//------------------------------------------------------------------------------
void OXReportControllerObserver::TogglePropertyListening(const uno::Reference< uno::XInterface > & Element)
{
	// listen at Container
	uno::Reference< container::XIndexAccess >  xContainer(Element, uno::UNO_QUERY);
	if (xContainer.is())
	{
		uno::Reference< uno::XInterface > xInterface;
		sal_Int32 nCount = xContainer->getCount();
		for(sal_Int32 i = 0;i != nCount;++i)
		{
			xInterface.set(xContainer->getByIndex( i ),uno::UNO_QUERY);
			TogglePropertyListening(xInterface);
		}
	}

	uno::Reference< beans::XPropertySet >  xSet(Element, uno::UNO_QUERY);
	if (xSet.is())
	{
		if (!m_pImpl->m_bReadOnly)
			xSet->addPropertyChangeListener( ::rtl::OUString(), this );
		else
			xSet->removePropertyChangeListener( ::rtl::OUString(), this );
	}
}


//------------------------------------------------------------------------------
void OXReportControllerObserver::switchListening( const uno::Reference< container::XIndexAccess >& _rxContainer, bool _bStartListening ) SAL_THROW(())
{
    OSL_PRECOND( _rxContainer.is(), "OXReportControllerObserver::switchListening: invalid container!" );
    if ( !_rxContainer.is() )
        return;

    try
    {
        // also handle all children of this element
		uno::Reference< uno::XInterface > xInterface;
        sal_Int32 nCount = _rxContainer->getCount();
		for(sal_Int32 i = 0;i != nCount;++i)
		{
			xInterface.set(_rxContainer->getByIndex( i ),uno::UNO_QUERY);
            if ( _bStartListening )
		        AddElement( xInterface );
            else
		        RemoveElement( xInterface );
	    }

        // be notified of any changes in the container elements
	    uno::Reference< container::XContainer > xSimpleContainer( _rxContainer, uno::UNO_QUERY );
        // OSL_ENSURE( xSimpleContainer.is(), "OXReportControllerObserver::switchListening: how are we expected to be notified of changes in the container?" );
	    if ( xSimpleContainer.is() )
        {
            if ( _bStartListening )
		        xSimpleContainer->addContainerListener( this );
            else
		        xSimpleContainer->removeContainerListener( this );
        }
    }
    catch( const uno::Exception& )
    {
    	DBG_UNHANDLED_EXCEPTION();
    }
}

//------------------------------------------------------------------------------
void OXReportControllerObserver::switchListening( const uno::Reference< uno::XInterface >& _rxObject, bool _bStartListening ) SAL_THROW(())
{
    OSL_PRECOND( _rxObject.is(), "OXReportControllerObserver::switchListening: how should I listen at a NULL object?" );

    try
    {
        if ( !m_pImpl->m_bReadOnly )
        {
            uno::Reference< beans::XPropertySet > xProps( _rxObject, uno::UNO_QUERY );
	        if ( xProps.is() )
            {
                if ( _bStartListening )
    		        xProps->addPropertyChangeListener( ::rtl::OUString(), this );
                else
    	    	    xProps->removePropertyChangeListener( ::rtl::OUString(), this );
            }
        }

        uno::Reference< util::XModifyBroadcaster > xBroadcaster( _rxObject, uno::UNO_QUERY );
        if ( xBroadcaster.is() )
        {
            if ( _bStartListening )
                xBroadcaster->addModifyListener( this );
            else
                xBroadcaster->removeModifyListener( this );
        }
    }
    catch( const uno::Exception& )
    {
        DBG_UNHANDLED_EXCEPTION();
    }
}

//------------------------------------------------------------------------------
void SAL_CALL OXReportControllerObserver::modified( const lang::EventObject& /*aEvent*/ ) throw (uno::RuntimeException)
{
    // implSetModified();
}

//------------------------------------------------------------------------------
void OXReportControllerObserver::AddElement(const uno::Reference< uno::XInterface >& _rxElement )
{
    // if ( !IsLocked() )
    // {
    m_aFormattedFieldBeautifier.notifyElementInserted(_rxElement);
    m_aFixedTextColor.notifyElementInserted(_rxElement);
    // }
    
    // if it's a container, start listening at all elements
    uno::Reference< container::XIndexAccess > xContainer( _rxElement, uno::UNO_QUERY );
	if ( xContainer.is() )
        switchListening( xContainer, true );

    switchListening( _rxElement, true );
}

//------------------------------------------------------------------------------
void OXReportControllerObserver::RemoveElement(const uno::Reference< uno::XInterface >& _rxElement)
{
    switchListening( _rxElement, false );

	uno::Reference< container::XIndexAccess > xContainer( _rxElement, uno::UNO_QUERY );
    if ( xContainer.is() )
        switchListening( xContainer, false );
}

// -----------------------------------------------------------------------------
::std::vector< uno::Reference< container::XChild> >::const_iterator OXReportControllerObserver::getSection(const uno::Reference<container::XChild>& _xContainer) const
{
    ::std::vector< uno::Reference< container::XChild> >::const_iterator aFind = m_pImpl->m_aSections.end();
    if ( _xContainer.is() )
    {
        aFind = ::std::find(m_pImpl->m_aSections.begin(),m_pImpl->m_aSections.end(),_xContainer);
    		
	    if ( aFind == m_pImpl->m_aSections.end() )
	    {
            uno::Reference<container::XChild> xParent(_xContainer->getParent(),uno::UNO_QUERY);
            aFind = getSection(xParent);
        }
    }
    return aFind;
}
// XContainerListener
//------------------------------------------------------------------------------
void SAL_CALL OXReportControllerObserver::elementInserted(const container::ContainerEvent& evt) throw(uno::RuntimeException)
{
	::vos::OClearableGuard aSolarGuard( Application::GetSolarMutex() );
    ::osl::MutexGuard aGuard( m_pImpl->m_aMutex );

	// neues Object zum lauschen
	uno::Reference< uno::XInterface >  xIface( evt.Element, uno::UNO_QUERY );
	if ( xIface.is() )
    {
        AddElement(xIface);
    }
}

//------------------------------------------------------------------------------
void SAL_CALL OXReportControllerObserver::elementReplaced(const container::ContainerEvent& evt) throw(uno::RuntimeException)
{
	::vos::OClearableGuard aSolarGuard( Application::GetSolarMutex() );
    ::osl::MutexGuard aGuard( m_pImpl->m_aMutex );

    uno::Reference< uno::XInterface >  xIface(evt.ReplacedElement,uno::UNO_QUERY);
	OSL_ENSURE(xIface.is(), "OXReportControllerObserver::elementReplaced: invalid container notification!");
	RemoveElement(xIface);

    xIface.set(evt.Element,uno::UNO_QUERY);
	AddElement(xIface);
}

//------------------------------------------------------------------------------
void SAL_CALL OXReportControllerObserver::elementRemoved(const container::ContainerEvent& evt) throw(uno::RuntimeException)
{
	::vos::OClearableGuard aSolarGuard( Application::GetSolarMutex() );
    ::osl::MutexGuard aGuard( m_pImpl->m_aMutex );

    uno::Reference< uno::XInterface >  xIface( evt.Element, uno::UNO_QUERY );
	if ( xIface.is() )
	{
        RemoveElement(xIface);
    }
}


} // namespace rptui



