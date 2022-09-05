/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

This file is part of FORCE - Framework for Operational Radiometric 
Correction for Environmental monitoring.

Copyright (C) 2013-2020 David Frantz

FORCE is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

FORCE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with FORCE.  If not, see <http://www.gnu.org/licenses/>.

+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/

/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This file contains functions for plugging-in python UDFs into FORCE
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
Copyright (C) 2020-2021 David Frantz, Andreas Rabe
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


#include "py-udf-hl.h"

#include <Python.h>
#include <numpy/ndarrayobject.h>
#include <numpy/ndarraytypes.h>

//#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION


typedef struct {
  npy_intp dim_nt[1];
  npy_intp dim_nb[1];
  PyArrayObject* year;
  PyArrayObject* month;
  PyArrayObject* day;
  PyArrayObject* sensor;
  PyArrayObject* bandname;
  PyArray_Descr *desc_sensor;
  PyArray_Descr *desc_bandname;
} py_dimlab_t;


py_dimlab_t python_label_dimensions(ard_t *ard, tsa_t *ts, int submodule, char *idx_name, int nb, int nt, par_udf_t *udf);
int date_from_bandname(date_t *date, char *bandname);

/** public functions
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


/** This function initializes the python interpreter, and defines a 
+++ function for wrapping the UDF code
--- phl:    HL parameters
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void register_python(par_hl_t *phl){
par_udf_t *udf;


  #ifdef FORCE_DEBUG
  printf("starting to register python interface\n");
  #endif

  // choose module
  if (phl->tsa.pyp.out){
    udf = &phl->tsa.pyp;
  } else if (phl->udf.pyp.out){
    udf = &phl->udf.pyp;
  } else {
    return;
  }


  Py_Initialize();

  import_array();

  PyRun_SimpleString("from multiprocessing.pool import Pool");
  PyRun_SimpleString("import numpy as np");
  PyRun_SimpleString("from datetime import date as Date");
  PyRun_SimpleString("import traceback");
  PyRun_SimpleString("import signal");

  PyRun_SimpleString("def init():                                                  \n"
                     "    np.seterr(all='ignore')                                  \n"
                     "    signal.signal(signal.SIGINT, signal.SIG_DFL)             \n");
  PyRun_SimpleString("init()");

  PyRun_SimpleString(
    "def forcepy_wrapper(args):                                                    \n"
    "    forcepy_udf, inarray, nband, date, sensor, bandname, nodata, nproc = args \n"
    "    outarray = np.full(shape=(nband,), fill_value=nodata, dtype=np.int16)     \n"
    "    forcepy_udf(inarray, outarray, date, sensor, bandname, nodata, nproc)     \n"
    "    return outarray                                                           \n");

  PyRun_SimpleString(
    "def forcepy_date2epoch(year, month, day):                                                  \n"
    "    dates = np.array(                                                                      \n"
    "        [np.datetime64(f'{str(y).zfill(4)}-{str(m).zfill(2)}-{str(d).zfill(2)}')           \n"
    "         for y, m, d in zip(year, month, day)])                                            \n"
    "    epoch = np.array([(date - np.datetime64('1970-01-01')).item().days for date in dates]) \n"
    "    return epoch                                                                           \n");

  PyRun_SimpleString(
    "def forcepy_init_(year, month, day, sensor, bandname):        \n"
    "    try:                                                      \n"
    "        if 'forcepy_init' not in globals():                   \n"
    "            print('forcepy_init not found.')                  \n"
    "            return None                                       \n"
    "        date = forcepy_date2epoch(year, month, day)           \n"
    "        out_bandnames_ = forcepy_init(date, sensor, bandname) \n"
    "        out_bandnames = list()                                \n"
    "        for s in out_bandnames_:                              \n"
    "            try: s = s.decode()                               \n"
    "            except: pass                                      \n"
    "            out_bandnames.append(str(s))                      \n"
    "        return out_bandnames                                  \n"
    "    except:                                                   \n"
    "        print(traceback.format_exc())                         \n"
    "        return None                                           \n");

  if (udf->type == _UDF_PIXEL_){
    PyRun_SimpleString(
      "def forcepy_(iblock, year, month, day, sensor, bandname, nodata, nband, nproc):           \n"
      "    try:                                                                                  \n"
      "        if 'forcepy_pixel' not in globals():                                              \n"
      "            print('forcepy_pixel not found.')                                             \n"
      "            return None                                                                   \n"
      "        nDates, nBands, nY, nX = iblock.shape                                             \n"
      "        pool = Pool(nproc, initializer=init)                                              \n"
      "        date = forcepy_date2epoch(year, month, day)                                       \n"
      "        argss = list()                                                                    \n"
      "        for yi in range(nY):                                                              \n"
      "            for xi in range(nX):                                                          \n"
      "                inarray = iblock[:, :, yi:yi+1, xi:xi+1]                                  \n"
      "                args = (forcepy_pixel, inarray, nband, date, sensor, bandname, nodata, 1) \n"
      "                argss.append(args)                                                        \n"
      "        res = pool.map(func=forcepy_wrapper, iterable=argss)                              \n"
      "        pool.close()                                                                      \n"
      "        del pool                                                                          \n"
      "        # reshape space dimensions                                                        \n"
      "        oblock = np.full(shape=(nband, nY, nX), fill_value=nodata, dtype=np.int16)        \n"
      "        i = 0                                                                             \n"
      "        for yi in range(nY):                                                              \n"
      "            for xi in range(nX):                                                          \n"
      "                oblock[:, yi, xi] = res[i]                                                \n"
      "                i += 1                                                                    \n"
      "        return oblock                                                                     \n"
      "    except:                                                                               \n"
      "        print(traceback.format_exc())                                                     \n"
      "        return None                                                                       \n");
  } else if (udf->type == _UDF_BLOCK_){
    PyRun_SimpleString(
      "def forcepy_(iblock, year, month, day, sensor, bandname, nodata, nband, nproc):        \n"
      "    try:                                                                               \n"
      "        if 'forcepy_block' not in globals():                                           \n"
      "            print('forcepy_block not found.')                                          \n"
      "            return None                                                                \n"
      "        nDates, nBands, nY, nX = iblock.shape                                          \n"
      "        date = forcepy_date2epoch(year, month, day)                                    \n"
      "        oblock = np.full(shape=(nband, nY, nX), fill_value=nodata, dtype=np.int16)     \n"
      "        forcepy_block(iblock, oblock, date, sensor, bandname, nodata, nproc)           \n"
      "        return oblock                                                                  \n"
      "    except:                                                                            \n"
      "        print(traceback.format_exc())                                                  \n"
      "        return None                                                                    \n");
  } else {
    printf("unknown UDF type.\n"); 
    exit(FAILURE);
  }

  #ifdef FORCE_DEBUG
  printf("finished to register python interface\n");
  #endif

  return;
}


/** This function cleans up the python interpreter
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void deregister_python(par_hl_t *phl){
par_udf_t *udf;


  #ifdef FORCE_DEBUG
  printf("starting to deregister python interface\n");
  #endif

  if (phl->tsa.pyp.out){
    udf = &phl->tsa.pyp;
  } else if (phl->udf.pyp.out){
    udf = &phl->udf.pyp;
  } else {
    return;
  }

  if (udf->out) Py_Finalize();

  #ifdef FORCE_DEBUG
  printf("finished to deregister python interface\n");
  #endif

  return;
}


/** This function extracts the date from the bandname
+++ If no date was detected, a dummy date is delivered
--- date:     date struct (returned)
--- bandname: basename of ARD image
+++ Return:   SUCCESS/FAILURE/CANCEL
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
int date_from_bandname(date_t *date, char *bandname){
char buffer[NPOW_10];
char *ptr = NULL;
const char *separator = " ,;-_.";
char cy[5], cm[3], cd[3];
date_t d;


  copy_string(buffer, NPOW_10, bandname);
  buffer[strcspn(buffer, "\r\n#")] = 0;
  ptr = strtok(buffer, separator);

  init_date(&d);

  if (strlen(ptr) != 8){
    *date = d;
    return CANCEL;
  }

  strncpy(cy, ptr,   4); cy[4] = '\0';
  strncpy(cm, ptr+4, 2); cm[2] = '\0';
  strncpy(cd, ptr+6, 2); cd[2] = '\0';

  set_date(&d, atoi(cy), atoi(cm), atoi(cd));

  #ifdef FORCE_DEBUG
  printf("date from pyp bandname 1: %04d (Y), %02d (M), %02d (D), %03d (DOY), %02d (W), %d (CE)\n",
    d.year, d.month, d.day, d.doy, d.week, d.ce);
  #endif

  if (d.year < 1900   || d.year > 2100){   init_date(&d); return CANCEL; }
  if (d.month < 1     || d.month > 12){    init_date(&d); return CANCEL; }
  if (d.day < 1       || d.day > 31){      init_date(&d); return CANCEL; }
  if (d.doy < 1       || d.doy > 365){     init_date(&d); return CANCEL; }
  if (d.week < 1      || d.week > 52){     init_date(&d); return CANCEL; }
  if (d.ce < 1900*365 || d.ce > 2100*365){ init_date(&d); return CANCEL; }

  #ifdef FORCE_DEBUG
  printf("date from pyp bandname 2: %04d (Y), %02d (M), %02d (D), %03d (DOY), %02d (W), %d (CE)\n",
    d.year, d.month, d.day, d.doy, d.week, d.ce);
  #endif

  *date = d;
  return SUCCESS;
}


/** This function initializes the python udf
--- ard:       ARD
--- ts:        pointer to instantly useable TSA image arrays
--- submodule: HLPS submodule
--- idx_name:  name of index for TSA submodule
--- nb:        number of bands
--- nt:        number of products over time
--- udf:       user-defined code parameters
+++ Return:    void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void init_pyp(ard_t *ard, tsa_t *ts, int submodule, char *idx_name, int nb, int nt, par_udf_t *udf){
FILE *fpy             = NULL;
py_dimlab_t pylab;
PyObject *main_module = NULL;
PyObject *main_dict   = NULL;
PyObject *py_fun      = NULL;
PyObject *py_return   = NULL;
PyObject *py_bandname = NULL;
PyObject *py_encoded  = NULL;
char *bandname = NULL;
date_t date;
int b;


  #ifdef FORCE_DEBUG
  printf("starting to initialize python interface\n");
  #endif

  //make sure bandnames and dates are NULL-initialized
  udf->bandname = NULL;
  udf->date     = NULL;

  if (!udf->out){
    udf->nb = 1;
    return;
  }


  main_module = PyImport_AddModule("__main__");
  main_dict   = PyModule_GetDict(main_module);

  pylab = python_label_dimensions(ard, ts, submodule, idx_name, nb, nt, udf);

  // parse the provided python function
  fpy = fopen(udf->f_code, "r");
  PyRun_SimpleFile(fpy, udf->f_code);

  py_fun = PyDict_GetItemString(main_dict, "forcepy_init_");
  if (py_fun == NULL){
    printf("Python error!\n");
    exit(FAILURE);}

  py_return = PyObject_CallFunctionObjArgs(
    py_fun, 
    pylab.year, pylab.month, pylab.day, 
    pylab.sensor, 
    pylab.bandname, 
    NULL);

  if (py_return == Py_None){
    printf("None returned from forcepy_init_. Check the python UDF code!\n");
    exit(FAILURE);}

  if (!PyList_Check(py_return)){
    printf("forcepy_init_ did not return a list. Check the python UDF code!\n");
    exit(FAILURE);}


  udf->nb = (int)PyList_Size(py_return);

  alloc_2D((void***)&udf->bandname, udf->nb, NPOW_10, sizeof(char));
  alloc((void**)&udf->date, udf->nb, sizeof(date_t));

  for (b=0; b<udf->nb; b++){

    py_bandname = PyList_GetItem(py_return, b);
    py_encoded  = PyUnicode_AsEncodedString(py_bandname, "UTF-8", "strict");
    if ((bandname = PyBytes_AsString(py_encoded)) == NULL){
      printf("forcepy_init_ did not return a list of strings. Check the python UDF code!\n");
      exit(FAILURE);}
    Py_DECREF(py_encoded);

    copy_string(udf->bandname[b], NPOW_10, bandname);
    
    date_from_bandname(&date, bandname);
    copy_date(&date, &udf->date[b]);

    #ifdef FORCE_DEBUG
    printf("bandname # %d: %s\n", b, udf->bandname[b]);
    print_date(&udf->date[b]);
    #endif

  }


  Py_DECREF(pylab.year);
  Py_DECREF(pylab.month);
  Py_DECREF(pylab.day);
  Py_DECREF(pylab.bandname);
  Py_DECREF(pylab.sensor);
  Py_DECREF(py_return);
  
  fclose(fpy);

  #ifdef FORCE_DEBUG
  printf("finished to initialize python interface\n");
  #endif

  return;
}


/** This function terminates the python udf
--- udf:    user-defined code parameters
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void term_pyp(par_udf_t *udf){


  #ifdef FORCE_DEBUG
  printf("starting to terminate python interface\n");
  #endif

  if (udf->bandname != NULL){
    free_2D((void**)udf->bandname, udf->nb); 
    udf->bandname = NULL;
  }

  if (udf->date != NULL){
    free((void*)udf->date); 
    udf->date = NULL;
  }

  #ifdef FORCE_DEBUG
  printf("finished to terminate python interface\n");
  #endif

  return;
}


/** This function labels the dimension of the UDF input data (time, band, sensor)
--- ard:       ARD
--- ts:        pointer to instantly useable TSA image arrays
--- submodule: HLPS submodule
--- idx_name:  name of index for TSA submodule
--- nb:        number of bands
--- nt:        number of products over time
--- udf:       user-defined code parameters
+++ Return:    void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
py_dimlab_t python_label_dimensions(ard_t *ard, tsa_t *ts, int submodule, char *idx_name, int nb, int nt, par_udf_t *udf){
py_dimlab_t pylab;
int b, t;
int* year_      = NULL;
int* month_     = NULL;
int* day_       = NULL;
char *sensor_   = NULL;
char *bandname_ = NULL;
date_t date;
char sensor[NPOW_04];
char bandname[NPOW_10];


  pylab.dim_nt[0] = nt;
  pylab.dim_nb[0] = nb;

  pylab.year     = (PyArrayObject *) PyArray_SimpleNew(1, pylab.dim_nt, NPY_INT);
  pylab.month    = (PyArrayObject *) PyArray_SimpleNew(1, pylab.dim_nt, NPY_INT);
  pylab.day      = (PyArrayObject *) PyArray_SimpleNew(1, pylab.dim_nt, NPY_INT);

  pylab.desc_sensor = PyArray_DescrNewFromType(NPY_STRING);
  pylab.desc_sensor->elsize = NPOW_04;
  pylab.sensor = (PyArrayObject *) PyArray_SimpleNewFromDescr(1, pylab.dim_nt, pylab.desc_sensor);

  pylab.desc_bandname = PyArray_DescrNewFromType(NPY_STRING);
  pylab.desc_bandname->elsize = NPOW_10;
  pylab.bandname = (PyArrayObject *) PyArray_SimpleNewFromDescr(1, pylab.dim_nb, pylab.desc_bandname);

  year_     = (int*)PyArray_DATA(pylab.year);
  month_    = (int*)PyArray_DATA(pylab.month);
  day_      = (int*)PyArray_DATA(pylab.day);
  sensor_   = (char*)PyArray_DATA(pylab.sensor);
  bandname_ = (char*)PyArray_DATA(pylab.bandname);


  // copy C data to python objects
  
  if (submodule == _HL_UDF_){

    for (t=0; t<nt; t++){
      date = get_brick_date(ard[t].DAT, 0);
      year_[t]  = date.year;
      month_[t] = date.month;
      day_[t]   = date.day;
      get_brick_sensor(ard[t].DAT, 0, sensor, NPOW_04);
      copy_string(sensor_, NPOW_04, sensor);
      sensor_ += NPOW_04;
    }

    for (b=0; b<nb; b++){
      get_brick_bandname(ard[0].DAT, b, bandname, NPOW_10);
      copy_string(bandname_, NPOW_10, bandname);
      bandname_ += NPOW_10;
    }

  } else if (submodule == _HL_TSA_){

    for (t=0; t<nt; t++){
      year_[t]  = ts->d_tsi[t].year;
      month_[t] = ts->d_tsi[t].month;
      day_[t]   = ts->d_tsi[t].day;
      copy_string(sensor_, NPOW_04, "BLEND");
      sensor_ += NPOW_04;
    }

    copy_string(bandname_, NPOW_10, idx_name);
    bandname_ += NPOW_10;

  } else {
    printf("unknown submodule. ");
    exit(FAILURE);
  }


  return pylab;
}


/** This function connects FORCE to plug-in python UDFs
--- ard:       pointer to instantly useable ARD image arrays
--- udf:       pointer to instantly useable UDF image arrays
--- ts:        pointer to instantly useable TSA image arrays
--- mask:      mask image
--- submodule: HLPS submodule
--- idx_name:  name of index for TSA submodule
--- nx:        number of columns
--- ny:        number of rows
--- nc:        number of cells
--- nb:        number of bands
--- nt:        number of time steps
--- nodata:    nodata value
--- p_udf:     user-defined code parameters
--- cthread:   number of computing threads
+++ Return:    SUCCESS/FAILURE
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
int python_udf(ard_t *ard, udf_t *udf_, tsa_t *ts, small *mask_, int submodule, char *idx_name, int nx, int ny, int nc, int nb, int nt, short nodata, par_udf_t *udf, int cthread){
int b, t, p;
size_t k;
py_dimlab_t pylab;
npy_intp dim_data[4] = { nt, nb, ny, nx };
FILE     *fpy         = NULL;
PyObject *main_module = NULL;
PyObject *main_dict   = NULL;
PyObject *py_fun      = NULL;
PyObject *py_nodata   = NULL;
PyObject *py_nproc    = NULL;
PyObject *py_nband    = NULL;
PyArrayObject* py_data     = NULL;
PyArrayObject *py_return   = NULL;
short* data_    = NULL;
short* return_  = NULL;


  if (submodule == _HL_UDF_ && udf_->pyp_ == NULL) return CANCEL;
  if (submodule == _HL_TSA_ &&   ts->pyp_ == NULL) return CANCEL;


  #ifdef FORCE_DEBUG
  printf("starting to run python interface\n");
  #endif

  main_module = PyImport_AddModule("__main__");
  main_dict   = PyModule_GetDict(main_module);

  pylab = python_label_dimensions(ard, ts, submodule, idx_name, nb, nt, udf);

  fpy = fopen(udf->f_code, "r");
  PyRun_SimpleFile(fpy, udf->f_code);

  py_fun = PyDict_GetItemString(main_dict, "forcepy_");
  if (py_fun == NULL){
    printf("Python error!\n");
    exit(FAILURE);}

  py_nodata = PyLong_FromLong(nodata);
  py_nproc = PyLong_FromLong(cthread);
  py_nband = PyLong_FromLong(udf->nb);

  py_data     = (PyArrayObject *) PyArray_SimpleNew(4, dim_data, NPY_INT16);
  data_     = (short*)PyArray_DATA(py_data);


  // copy C data to python objects
  
  if (submodule == _HL_UDF_){

    for (t=0, k=0; t<nt; t++){
      for (b=0; b<nb; b++){
        for (p=0; p<nc; p++){
          if (!ard[t].msk[p]){
            data_[k++] = nodata;
          } else {
            data_[k++] = ard[t].dat[b][p];
          }
        }
      }
    }

  } else if (submodule == _HL_TSA_){

    for (t=0; t<nt; t++){
      memcpy(data_, ts->tsi_[t], sizeof(short)*nc);
      data_ += nc;
    }

  } else {
    printf("unknown submodule. ");
    exit(FAILURE);
  }


  // fire up python
  py_return = (PyArrayObject *) PyObject_CallFunctionObjArgs(
    py_fun, 
    py_data, 
    pylab.year, pylab.month, pylab.day, 
    pylab.sensor, 
    pylab.bandname, 
    py_nodata, 
    py_nband, 
    py_nproc, 
    NULL);

  if (py_return == Py_None){
    printf("None returned from python. Check the python UDF code!\n");
    exit(FAILURE);}


  // copy to output brick
  return_ = (short*)PyArray_DATA(py_return);

  if (submodule == _HL_UDF_){

    for (b=0; b<udf->nb; b++){
      memcpy(udf_->pyp_[b], return_, sizeof(short)*nc);
      return_ += nc;
    }

  } else if (submodule == _HL_TSA_){

    for (b=0; b<udf->nb; b++){
      memcpy(ts->pyp_[b], return_, sizeof(short)*nc);
      return_ += nc;
    }

  } else {
    printf("unknown submodule.\n");
    exit(FAILURE);
  }



  // clean
  Py_DECREF(py_return);
  Py_DECREF(py_data);
  Py_DECREF(py_nodata);
  Py_DECREF(py_nband);
  Py_DECREF(py_nproc);
  Py_DECREF(pylab.year);
  Py_DECREF(pylab.month);
  Py_DECREF(pylab.day);
  Py_DECREF(pylab.bandname);
  Py_DECREF(pylab.sensor);


  fclose(fpy);

  #ifdef FORCE_DEBUG
  printf("finished to run python interface\n");
  #endif

  return SUCCESS;
}

