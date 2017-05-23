#!/usr/bin/env python

import os

# Make a copy of the local home because the regressions run
# in place for now
jobtype_ocr_regression = {
    'name': 'ocr-regression',
    'isLocal': True,
    'run-cmd': '${JJOB_INITDIR_OCR}/ocr/jenkins/scripts/regression.sh',
    'param-cmd': '${JJOB_INITDIR_OCR}/ocr/jenkins/scripts/regression.sh _params',
    'keywords': ('ocr', 'regression', 'percommit'),
    'timeout': 240,
    'sandbox': ('local', 'shared', 'shareOK'),
    'req-repos': ('ocr',)
}

jobtype_ocr_tg_regression = {
    'name': 'ocr-tg-regression',
    'isLocal': True,
    'run-cmd': '${JJOB_INITDIR_OCR}/ocr/jenkins/scripts/regression.sh',
    'param-cmd': '${JJOB_INITDIR_OCR}/ocr/jenkins/scripts/regression.sh _params',
    'depends': ('ocr-build-builder-ce', 'ocr-build-builder-xe',
                'ocr-build-tg-ce', 'ocr-build-tg-xe', 'apps-tg-init-job', ),
    'keywords': ('tg', 'ocr', 'regression'),
    'timeout': 8000,
    'sandbox': ('local', 'shared', 'shareOK'),
    # Need the apps repo because we're using the apps
    # makefile infrastructure to run TG tests
    'req-repos': ('apps', 'ocr', 'tg',),
    'env-vars': { 'TG_INSTALL': '${JJOB_ENVDIR}',
                  'TG_ROOT': '${JJOB_INITDIR_TG}/tg',
                  'APPS_ROOT': '${JJOB_SHARED_HOME}/apps/apps',
                  'APPS_LIBS_ROOT': '${APPS_ROOT}/libs/src',
                  'APPS_ROOT_PRIV': '${JJOB_PRIVATE_HOME}/apps/apps',
                  'APPS_LIBS_INSTALL_ROOT': '${APPS_ROOT}/apps/apps/libs/install/',
                  'OCR_ROOT': '${JJOB_PRIVATE_HOME}/ocr/ocr',
                  'OCR_INSTALL': '${JJOB_SHARED_HOME}/ocr/ocr/install',
                  'OCR_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/ocr/ocr/build',
                  'APPS_MAKEFILE': '${APPS_ROOT}/${T_PATH}/Makefile',
                  # Note 1: The non-regression tests script does some additional per-test folder setup.
                  # Hence we do not set WORKLOAD_SRC to use JJOB_INITDIR_OCR and rather
                  # let the script automatically use the build folder to do the setup.
                  # Note 2: Important to set here to get the right job home.
                  # They are further specialized in the non-regression test script
                  'WORKLOAD_BUILD_ROOT': '${JJOB_PRIVATE_HOME}/ocr/ocr/tests',
                  'WORKLOAD_INSTALL_ROOT': '${JJOB_SHARED_HOME}/ocr/ocr/tests'}
}

# Specific jobs

job_ocr_regression_x86_pthread_x86 = {
    'name': 'ocr-regression-x86',
    'depends': ('ocr-build-x86',),
    'jobtype': 'ocr-regression',
    'run-args': 'x86 jenkins-common-8w-regularDB.cfg regularDB',
    'sandbox': ('inherit0',)
}

job_ocr_regression_x86_pthread_x86_lockableDB = {
    'name': 'ocr-regression-x86-lockableDB',
    'depends': ('ocr-build-x86',),
    'jobtype': 'ocr-regression',
    'run-args': 'x86 jenkins-common-8w-lockableDB.cfg lockableDB',
    'sandbox': ('inherit0',)
}

#job_ocr_regression_x86_pthread_tg_regularDB = {
#    'name': 'ocr-regression-tg-x86-regularDB',
#    'depends': ('ocr-build-tg-x86',),
#    'jobtype': 'ocr-regression',
#    'run-args': 'tg-x86 jenkins-1block-regularDB.cfg regularDB',
#    'timeout': 400,
#    'sandbox': ('inherit0',),
#    'env-vars': { 'TG_INSTALL': '${JJOB_ENVDIR}' }
#}
#
#job_ocr_regression_x86_pthread_tg_lockableDB = {
#    'name': 'ocr-regression-tg-x86-lockableDB',
#    'depends': ('ocr-build-tg-x86',),
#    'jobtype': 'ocr-regression',
#    'run-args': 'tg-x86 jenkins-1block-lockableDB.cfg lockableDB',
#    'timeout': 400,
#    'sandbox': ('inherit0',),
#    'env-vars': { 'TG_INSTALL': '${JJOB_ENVDIR}' }
#}

job_ocr_regression_fsim_tg = {
    'name': 'ocr-regression-fsim-tg',
    'jobtype': 'ocr-tg-regression',
    # TG gets default CFGs from install folder. IGNORED_CFG_ARG has no meaning.
    'run-args': 'tg IGNORED_CFG_ARG lockableDB',
    'sandbox': ('inherit0',)
}

#TODO: not sure how to not hardcode MPI_ROOT here
job_ocr_regression_x86_pthread_mpi_lockableDB = {
    'name': 'ocr-regression-x86-mpi-lockableDB',
    'depends': ('ocr-build-x86-mpi',),
    'jobtype': 'ocr-regression',
    'run-args': 'x86-mpi jenkins-x86-mpi.cfg lockableDB',
    'sandbox': ('inherit0',),
    'env-vars': {'MPI_ROOT': '/opt/intel/tools/impi/5.1.1.109/intel64',
                 'PATH': '${MPI_ROOT}/bin:'+os.environ['PATH'],
                 'LD_LIBRARY_PATH': '${MPI_ROOT}/lib64',}
}

#TODO: not sure how to not hardcode MPI_ROOT here
# Bug #945 re-enable when tests are fixed
#job_ocr_regression_x86_pthread_mpi_st_lockableDB = {
#    'name': 'ocr-regression-x86-mpi-st-lockableDB',
#    'depends': ('ocr-build-x86-mpi',),
#    'jobtype': 'ocr-regression',
#    'run-args': 'x86-mpi jenkins-x86-mpi-st.cfg lockableDB',
#    'sandbox': ('inherit0',),
#    'env-vars': {'MPI_ROOT': '/opt/intel/tools/impi/5.1.1.109/intel64',
#                 'PATH': '${MPI_ROOT}/bin:'+os.environ['PATH'],
#                 'LD_LIBRARY_PATH': '${MPI_ROOT}/lib64',}
#}

#TODO: not sure how to not hardcode GASNET_ROOT here
job_ocr_regression_x86_pthread_gasnet_lockableDB = {
    'name': 'ocr-regression-x86-gasnet-lockableDB',
    'depends': ('ocr-build-x86-gasnet',),
    'jobtype': 'ocr-regression',
    'run-args': 'x86-gasnet jenkins-x86-gasnet.cfg lockableDB',
    'sandbox': ('inherit0',),
    'env-vars': {'MPI_ROOT': '/opt/intel/tools/impi/5.1.1.109/intel64',
                 'GASNET_ROOT': '/opt/rice/GASNet/1.24.0-impi',
                 'PATH': '${GASNET_ROOT}/bin:${MPI_ROOT}/bin:'+os.environ['PATH'],
                 'GASNET_CONDUIT': 'ibv',
                 'GASNET_TYPE': 'par',
                 'GASNET_EXTRA_LIBS': '-L/usr/lib64 -lrt -libverbs',
                 'CC': 'mpicc', # gasnet built with mpi
                 # picked up by non-regression test script
                 'OCR_LDFLAGS': '-L${GASNET_ROOT}/lib -lgasnet-${GASNET_CONDUIT}-${GASNET_TYPE} ${GASNET_EXTRA_LIBS}',}
}
