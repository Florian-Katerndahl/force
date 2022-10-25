/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

This file is part of FORCE - Framework for Operational Radiometric 
Correction for Environmental monitoring.

Copyright (C) 2013-2022 David Frantz

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
This file contains functions for sampling of features
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


#include "sample-hl.h"


/** private functions
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

double **parse_coord_list(char *fname, size_t *ncoord);


/** This function reads coordinates from a text file. Put X-coords in 1st
+++ column and Y-coords in 2nd column. Coordinates must be in geographic 
+++ decimal degree notation (South and West coordinates are negative). Do
+++ not use a header.
--- fname:  text file containing the coordinates
--- ncoord: number of coordinate pairs (returned)
+++ Return: array with coordinates
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
/**double **parse_coord_list(char *fname, size_t *ncoord){
FILE *fp;
char  buffer[NPOW_10] = "\0";
char *tag = NULL;
const char *separator = " \t";
double **coords = NULL;
size_t k = 0;
size_t bufsize = NPOW_10;


  alloc_2D((void***)&coords, 3, bufsize, sizeof(double));

  // open file
  if (!(fp = fopen(fname, "r"))){
    printf("unable to open coordinate file. "); return NULL;}

  // process line by line
  while (fgets(buffer, NPOW_10, fp) != NULL){
    
    tag = strtok(buffer, separator);

    coords[0][k] = atof(tag); tag = strtok(NULL, separator);
    coords[1][k] = atof(tag); tag = strtok(NULL, separator);
    coords[2][k] = atof(tag); tag = strtok(NULL, separator);
    k++;

    // if extremely large size, attempt to increase buffer size
    if (k >= bufsize) {
      //printf("reallocate.. %lu %lu\n", k, bufsize);
      re_alloc_2D((void***)&coords, 3, bufsize, 3, bufsize*2, sizeof(coord_t));
      bufsize *= 2;
    }

  }

  fclose(fp);


  *ncoord = k;
  return coords;
}**/


void append_table(char *fname, bool *allow, double **tab, int nrow, int ncol, int decimals);

void append_table(char *fname, bool *allow, double **tab, int nrow, int ncol, int decimals){
int row, col;
FILE *fp = NULL;


  if (!(fp = fopen(fname, "a"))){
    printf("unable to open %s. ", fname); exit(FAILURE);}

  for (row=0; row<nrow; row++){

    if (allow[row]){

      fprintf(fp, "%.*f", decimals, tab[row][0]);
      for (col=1; col<ncol; col++) fprintf(fp, " %.*f", decimals, tab[row][col]);
      fprintf(fp, "\n");
    }

  }

  fclose(fp);

  return;
}






/** public functions
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


/** This function is the entry point to the sampling module
--- features: input features
--- mask:      mask image
--- nf:        number of features
--- phl:       HL parameters
--- cube:      datacube definition
--- nproduct:  number of output bricks (returned)
+++ Return:    empty bricks
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
brick_t **sample_points(ard_t *features, brick_t *mask, int nf, par_hl_t *phl, aux_smp_t *smp, cube_t *cube, int *nproduct){
small *mask_ = NULL;
int f, r, s, i, j, p;
int cx, cy, chunk, tx, ty;
int nr;
double res;
coord_t smp_map, smp_map_ul;
int smp_tx, smp_ty, smp_chunk;
int smp_tj, smp_ti;
int error = 0, found = 0, added = 0;
bool *copied = NULL, valid;
double **smp_features = NULL;
double **smp_response = NULL;
coord_t geo_ul, geo_ur, geo_lr, geo_ll;
coord_t geo_upper, geo_lower, geo_left, geo_right;
double minx, maxx, miny, maxy; 


  // if no sample is left, skip all
  if (smp->nleft == 0){
    *nproduct = 0;
    return NULL;
  }


  // import bricks
  cx    = get_brick_chunkncols(features[0].DAT);
  cy    = get_brick_chunknrows(features[0].DAT);
  res   = get_brick_res(features[0].DAT);
  chunk = get_brick_chunk(features[0].DAT);
  tx    = get_brick_tilex(features[0].DAT);
  ty    = get_brick_tiley(features[0].DAT);

//  nodata = get_brick_nodata(features[0].DAT, 0);

  // import mask (if available)
  if (mask != NULL){
    if ((mask_ = get_band_small(mask, 0)) == NULL){
      printf("Error getting processing mask."); 
      *nproduct = 0;
      return NULL;}
  }


  nr = smp->nr-2;

  alloc((void**)&copied,           smp->ns,     sizeof(bool));
  alloc_2D((void***)&smp_features, smp->ns, nf, sizeof(double));
  alloc_2D((void***)&smp_response, smp->ns, nr, sizeof(double));


  if (phl->smp.projected){

    // corner coordinates of chunk
    minx = get_brick_x(features[0].DAT, 0);
    maxx = get_brick_x(features[0].DAT, cx);
    miny = get_brick_y(features[0].DAT, (chunk+1)*cy);
    maxy = get_brick_y(features[0].DAT, chunk*cy);

  } else {

    // corner coordinates of chunk
    get_brick_geo(features[0].DAT, 0,    chunk*cy,        &geo_ul.x, &geo_ul.y);
    get_brick_geo(features[0].DAT, cx,   chunk*cy,        &geo_ur.x, &geo_ur.y);
    get_brick_geo(features[0].DAT, 0,    (chunk+1)*cy,    &geo_ll.x, &geo_ll.y);
    get_brick_geo(features[0].DAT, cx,   (chunk+1)*cy,    &geo_lr.x, &geo_lr.y);

    // min/max coordinates
    minx = MIN(geo_ul.x, geo_ll.x);
    maxx = MAX(geo_ur.x, geo_lr.x);
    miny = MIN(geo_ll.y, geo_lr.y);
    maxy = MAX(geo_ul.y, geo_ur.y);

    // edge coordinates of chunk
    get_brick_geo(features[0].DAT, cx/2, chunk*cy,        &geo_upper.x, &geo_upper.y);
    get_brick_geo(features[0].DAT, cx/2, (chunk+1)*cy,    &geo_lower.x, &geo_lower.y);
    get_brick_geo(features[0].DAT, 0,    chunk*cy + cy/2, &geo_left.x,  &geo_left.y);
    get_brick_geo(features[0].DAT, cx,   chunk*cy + cy/2, &geo_right.x, &geo_right.y);

    // if edge coordinates are outside of corner min/max, expand the box a bit
    if (geo_left.x  < minx) minx -= 2*fabs(geo_left.x -minx);
    if (geo_right.x > maxx) maxx += 2*fabs(geo_right.x-maxx);
    if (geo_lower.y < miny) miny -= 2*fabs(geo_lower.y-miny);
    if (geo_upper.y > maxy) maxy += 2*fabs(geo_upper.y-maxy);

  }


  #pragma omp parallel private(smp_map,smp_map_ul,smp_tx,smp_ty,smp_tj,smp_ti,smp_chunk,j,i,p,f,r,valid) shared(tx,ty,chunk,res,cx,cy,nf,nr,minx,maxx,miny,maxy,copied,smp_features,smp_response,smp,features,mask_,cube,phl) reduction(+: error, found, added) default(none)
  {

    #pragma omp for
    for (s=0; s<smp->ns; s++){

      if (smp->visited[s]) continue;

      if (smp->tab[s][_X_] < minx) continue;
      if (smp->tab[s][_X_] > maxx) continue;
      if (smp->tab[s][_Y_] < miny) continue;
      if (smp->tab[s][_Y_] > maxy) continue;
      
      if (phl->smp.projected){

        smp_map.x = smp->tab[s][_X_];
        smp_map.y = smp->tab[s][_Y_];

      } else {

        // get target coordinates in target css coordinates
        if ((warp_geo_to_any(smp->tab[s][_X_], smp->tab[s][_Y_], &smp_map.x, &smp_map.y, cube->proj)) == FAILURE){
          printf("Computing target coordinates in dst_srs failed!\n"); 
          error++;
          continue;
        }

      }

      // find the tile the sample falls into
      tile_find(smp_map.x, smp_map.y, &smp_map_ul.x, &smp_map_ul.y, &smp_tx, &smp_ty, cube);

      // if not in current tile, skip
      if (smp_tx != tx || smp_ty != ty) continue;

      // find pixel in tile
      smp_tj = (int)((smp_map.x-smp_map_ul.x)/res);
      smp_ti = (int)((smp_map_ul.y-smp_map.y)/res);

      // find chunk in tile
      smp_chunk = (int)(smp_ti/cy);

      // if not in current chunk, skip
      if (smp_chunk != chunk) continue;

      // find pixel in chunk
      j = smp_tj;
      i = smp_ti - chunk*cy;
      p = i*cx+j;

      // skip pixels that are masked
      if (mask_ != NULL && !mask_[p]) continue;

      // extract
      for (f=0, valid=true; f<nf; f++){
        smp_features[s][f] = features[f].dat[0][p];
        if (!features[f].msk[p] && phl->ftr.exclude) valid = false;
      }
     for (r=0; r<nr; r++) smp_response[s][r] = smp->tab[s][_Z_+r];


      // we are done with this sample
      smp->visited[s] = true;
      found++;

      if (!valid) continue;

      copied[s] = true;
      added++;

    }

  }

  if (error > 0) printf("there were %d errors in coordinate conversion..\n", error);

  smp->nleft -= found;


  if (added > 0){
    append_table(phl->smp.f_sample,   copied, smp_features, smp->ns, nf, 0);
    append_table(phl->smp.f_response, copied, smp_response, smp->ns, nr, 6);
    append_table(phl->smp.f_coords,   copied, smp->tab,     smp->ns, 2,  6);
  }


  #ifdef FORCE_DEBUG
  if (added > 0) printf("Added %d samples in Tile X%04d_Y%04d Chunk %03d.\n", 
    added, tx, ty, chunk);
  #endif


  free((void*)copied);
  free_2D((void**)smp_response, smp->ns);
  free_2D((void**)smp_features, smp->ns);

  *nproduct = 0;
  return NULL;
}

