#**************************************************************
#  
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#  
#    http://www.apache.org/licenses/LICENSE-2.0
#  
#  Unless required by applicable law or agreed to in writing,
#  software distributed under the License is distributed on an
#  "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
#  KIND, either express or implied.  See the License for the
#  specific language governing permissions and limitations
#  under the License.
#  
#**************************************************************



PRJ=..

ENABLE_EXCEPTIONS=TRUE
VISIBILITY_HIDDEN=TRUE
PRJNAME=scripting
TARGET=scriptframe
USE_DEFFILE=    TRUE
NO_BSYMBOLIC=   TRUE

# --- Settings -----------------------------------------------------

.INCLUDE :  settings.mk

# --- Allgemein ----------------------------------------------------
LIB1TARGET=	 $(SLB)$/$(TARGET).lib
LIB1FILES=   $(SLB)/provider.lib


SHL1TARGET= $(TARGET)
SHL1IMPLIB= $(TARGET)

SHL1STDLIBS+=\
	$(TOOLSLIB) \
	$(CPPULIB) \
	$(CPPUHELPERLIB) \
	$(COMPHELPERLIB) \
	$(UCBHELPERLIB) \
	$(SALLIB)


SHL1LIBS=	$(SLB)$/$(TARGET).lib

SHL1DEF=    $(MISC)$/$(SHL1TARGET).def

DEF1NAME	=$(SHL1TARGET)
DEF1EXPORTFILE= exports.dxp
# --- Targets ------------------------------------------------------


.INCLUDE :  target.mk

ALLTAR : $(MISC)/scriptframe.component

$(MISC)/scriptframe.component .ERRREMOVE : \
        $(SOLARENV)/bin/createcomponent.xslt scriptframe.component
    $(XSLTPROC) --nonet --stringparam uri \
        '$(COMPONENTPREFIX_BASIS_NATIVE)$(SHL1TARGETN:f)' -o $@ \
        $(SOLARENV)/bin/createcomponent.xslt scriptframe.component
