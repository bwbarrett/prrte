#
# Copyright (c) 2011      Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2013      Los Alamos National Security, LLC.  All rights
#                         reserved.
# Copyright (c) 2014      Intel, Inc.  All rights reserved.
# Copyright (c) 2017-2020 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
# 
# $HEADER$
#

AM_CPPFLAGS = $(grpcomm_bmg_CPPFLAGS)

sources = \
        grpcomm_bmg.h \
        grpcomm_bmg_module.c \
        grpcomm_bmg_component.c

# Make the output library in this bmgory, and name it either
# # mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# # (for static builds)

if MCA_BUILD_prte_grpcomm_bmg_DSO
component_noinst =
component_install = mca_grpcomm_bmg.la
else
component_noinst = libmca_grpcomm_bmg.la
component_install =
endif

mcacomponentdir = $(prtelibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_grpcomm_bmg_la_SOURCES = $(sources)
mca_grpcomm_bmg_la_LDFLAGS = -module -avoid-version
mca_grpcomm_bmg_la_LIBADD = $(top_builddir)/src/libprrte.la

noinst_LTLIBRARIES = $(component_noinst)
libmca_grpcomm_bmg_la_SOURCES =$(sources)
libmca_grpcomm_bmg_la_LDFLAGS = -module -avoid-version

