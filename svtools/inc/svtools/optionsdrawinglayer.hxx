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



#ifndef INCLUDED_SVTOOLS_OPTIONSDRAWINGLAYER_HXX
#define INCLUDED_SVTOOLS_OPTIONSDRAWINGLAYER_HXX

//_________________________________________________________________________________________________________________
//	includes
//_________________________________________________________________________________________________________________

#include "svtools/svtdllapi.h"
#include <sal/types.h>
#include <osl/mutex.hxx>
#include <rtl/ustring.hxx>
#include <tools/color.hxx>

//_________________________________________________________________________________________________________________
//	forward declarations
//_________________________________________________________________________________________________________________

/*-************************************************************************************************************//**
	@short			forward declaration to our private date container implementation
	@descr			We use these class as internal member to support small memory requirements.
					You can create the container if it is necessary. The class which use these mechanism
					is faster and smaller then a complete implementation!
*//*-*************************************************************************************************************/

class SvtOptionsDrawinglayer_Impl;

//_________________________________________________________________________________________________________________
//	declarations
//_________________________________________________________________________________________________________________

/*-************************************************************************************************************//**
	@short			collect informations about startup features
	@descr          -

	@implements		-
	@base			-

	@devstatus		ready to use
*//*-*************************************************************************************************************/

class SVT_DLLPUBLIC SvtOptionsDrawinglayer
{
	//-------------------------------------------------------------------------------------------------------------
	//	public methods
	//-------------------------------------------------------------------------------------------------------------

	public:

		//---------------------------------------------------------------------------------------------------------
		//	constructor / destructor
		//---------------------------------------------------------------------------------------------------------

		/*-****************************************************************************************************//**
			@short		standard constructor and destructor
			@descr		This will initialize an instance with default values.
						We implement these class with a refcount mechanism! Every instance of this class increase it
						at create and decrease it at delete time - but all instances use the same data container!
						He is implemented as a static member ...

			@seealso	member m_nRefCount
			@seealso	member m_pDataContainer

			@param		-
			@return		-

			@onerror	-
		*//*-*****************************************************************************************************/

         SvtOptionsDrawinglayer();
        ~SvtOptionsDrawinglayer();

		//---------------------------------------------------------------------------------------------------------
		//	interface
		//---------------------------------------------------------------------------------------------------------

		/*-****************************************************************************************************//**
			@short		interface methods to get and set value of config key "org.openoffice.Office.Common/Drawinglayer/..."
			@descr      These options describe internal states to enable/disable features of installed office.

						IsOverlayBuffer()
						SetOverlayBuffer()		=>	Activate this field for letting Overlay use a buffer

						IsPaintBuffer()
						SetPaintBuffer()		=>	Activate this field for letting Paint use a prerender buffer

						GetStripeColorA()
						SetStripeColorA()		=>	Set first of two colors which overlay uses to draw stripes

						GetStripeColorB()
						SetStripeColorB()		=>	Set second of two colors which overlay uses to draw stripes

						GetStripeLength()
						SetStripeLength()		=>	Set length of a single stripe in pixels

			@seealso	configuration package "org.openoffice.Office.Common/Drawinglayer"
		*//*-*****************************************************************************************************/

		sal_Bool	IsOverlayBuffer() const;
		sal_Bool	IsPaintBuffer() const;
		Color		GetStripeColorA() const;
		Color		GetStripeColorB() const;
		sal_uInt16	GetStripeLength() const;

		void		SetOverlayBuffer( sal_Bool bState );
		void		SetPaintBuffer( sal_Bool bState );
		void		SetStripeColorA( Color aColor );
		void		SetStripeColorB( Color aColor );
		void		SetStripeLength( sal_uInt16 nLength );

		// #i73602#
		sal_Bool	IsOverlayBuffer_Calc() const;
		sal_Bool	IsOverlayBuffer_Writer() const;
		sal_Bool	IsOverlayBuffer_DrawImpress() const;
		void		SetOverlayBuffer_Calc( sal_Bool bState );
		void		SetOverlayBuffer_Writer( sal_Bool bState );
		void		SetOverlayBuffer_DrawImpress( sal_Bool bState );

		// #i74769#, #i75172#
		sal_Bool	IsPaintBuffer_Calc() const;
		sal_Bool	IsPaintBuffer_Writer() const;
		sal_Bool	IsPaintBuffer_DrawImpress() const;
		void		SetPaintBuffer_Calc( sal_Bool bState );
		void		SetPaintBuffer_Writer( sal_Bool bState );
		void		SetPaintBuffer_DrawImpress( sal_Bool bState );

		// #i4219#
		sal_uInt32 GetMaximumPaperWidth() const;
		sal_uInt32 GetMaximumPaperHeight() const;
		sal_uInt32 GetMaximumPaperLeftMargin() const;
		sal_uInt32 GetMaximumPaperRightMargin() const;
		sal_uInt32 GetMaximumPaperTopMargin() const;
		sal_uInt32 GetMaximumPaperBottomMargin() const;
		
		void SetMaximumPaperWidth(sal_uInt32 nNew);
		void SetMaximumPaperHeight(sal_uInt32 nNew);
		void SetMaximumPaperLeftMargin(sal_uInt32 nNew);
		void SetMaximumPaperRightMargin(sal_uInt32 nNew);
		void SetMaximumPaperTopMargin(sal_uInt32 nNew);
		void SetMaximumPaperBottomMargin(sal_uInt32 nNew);

		// #i95644# helper to check if AA is allowed on this system. Currently, for WIN its disabled 
		// and OutDevSupport_TransparentRect is checked (this  hits XRenderExtension, e.g. 
		// currently for SunRay as long as not supported there)
		sal_Bool IsAAPossibleOnThisSystem() const;

		// primitives
		sal_Bool	IsAntiAliasing() const;
		sal_Bool	IsSnapHorVerLinesToDiscrete() const;
		sal_Bool	IsSolidDragCreate() const;
		sal_Bool	IsRenderDecoratedTextDirect() const;
		sal_Bool	IsRenderSimpleTextDirect() const;
		sal_uInt32  GetQuadratic3DRenderLimit() const;
		sal_uInt32  GetQuadraticFormControlRenderLimit() const;

        void		SetAntiAliasing( sal_Bool bState );
        void		SetSnapHorVerLinesToDiscrete( sal_Bool bState );
		void		SetSolidDragCreate( sal_Bool bState );
        void		SetRenderDecoratedTextDirect( sal_Bool bState );
        void		SetRenderSimpleTextDirect( sal_Bool bState );
   		void        SetQuadratic3DRenderLimit(sal_uInt32 nNew);
   		void        SetQuadraticFormControlRenderLimit(sal_uInt32 nNew);

		// #i97672# selection settings
		sal_Bool	IsTransparentSelection() const;
		sal_uInt16	GetTransparentSelectionPercent() const;
        sal_uInt16	GetSelectionMaximumLuminancePercent() const;

        // get system hilight color, limited to the maximum allowed luminance
        // (defined in GetSelectionMaximumLuminancePercent() in SvtOptionsDrawinglayer,
        // combined with Application::GetSettings().GetStyleSettings().GetHighlightColor())
        Color getHilightColor() const;

		void		SetTransparentSelection( sal_Bool bState );
		void		SetTransparentSelectionPercent( sal_uInt16 nPercent );
		void		SetSelectionMaximumLuminancePercent( sal_uInt16 nPercent );

	//-------------------------------------------------------------------------------------------------------------
	//	private methods
	//-------------------------------------------------------------------------------------------------------------

    private:

		/*-****************************************************************************************************//**
			@short		return a reference to a static mutex
			@descr		These class use his own static mutex to be threadsafe.
						We create a static mutex only for one ime and use at different times.

			@seealso	-

			@param		-
			@return		A reference to a static mutex member.

			@onerror	-
		*//*-*****************************************************************************************************/
		SVT_DLLPRIVATE static ::osl::Mutex& GetOwnStaticMutex();

	//-------------------------------------------------------------------------------------------------------------
	//	private member
	//-------------------------------------------------------------------------------------------------------------

	private:

		/*Attention

			Don't initialize these static member in these header!
			a) Double dfined symbols will be detected ...
			b) and unresolved externals exist at linking time.
			Do it in your source only.
		 */

    	static SvtOptionsDrawinglayer_Impl*		m_pDataContainer	;	/// impl. data container as dynamic pointer for smaller memory requirements!
		static sal_Int32						m_nRefCount			;	/// internal ref count mechanism

};		// class SvtOptionsDrawinglayer

#endif	// #ifndef INCLUDED_SVTOOLS_OPTIONSDRAWINGLAYER_HXX
