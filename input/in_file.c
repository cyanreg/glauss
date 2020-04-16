/*
 * glauss is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * glauss is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with glauss.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tgmath.h>
#include "input/in_file.h"
#include "shared/options.h"
#include "shared/msg_phys.h"
#include "physics/physics_aux.h"

enum ext_file in_file_ext(const char *filename)
{
    enum ext_file filetype = MOL_UNKNOWN;
    if (strstr(filename, "xyz")!=NULL) {
        filetype = MOL_XYZ;
    } else if (strstr(filename, "pdb")!=NULL) {
        filetype = MOL_PDB;
    } else if (strstr(filename, "obj")!=NULL) {
        filetype = MOL_OBJ;
    } else {
        pprint_err("[IN] Filetype of %s not recognized!\n", filename);
        exit(1);
    }
    return filetype;
}

int in_probe_file(const char *filename)
{
    char str[500];
    int counter = 0;
    FILE *inprep = fopen(filename, "r");
    /*
    * XYZ files have total number of atoms in their first line.
    * PDB files contain an incrementing index, but I'd like to avoid sscanf.
    */
    switch (in_file_ext(filename)) {
        case MOL_XYZ:
            (void)fgets(str, sizeof(str), inprep);
            fclose(inprep);
            sscanf(str, "%i", &counter);
            break;
        case MOL_OBJ:
            while (fgets (str, sizeof(str), inprep)!= NULL) {
                if (!strstr(str, "#")) {
                    if (!strncmp(str, "v ", 2))
                        counter++;
                }
            }
            break;
        case MOL_PDB:
            while (fgets(str, sizeof(str), inprep)) {
                if (!strstr(str, "#")) {
                    if (!strncmp(str, "ATOM", 4))
                        counter++;
                }
            }
            break;
        default:
            break;
    }
    if (option->skip_model_vec) {
        counter/=option->skip_model_vec;
    }
    return counter;
}

int in_read_file(phys_obj *object, int *i, in_file *file)
{
    char str[500] = {0}, atom[2] = {0}, pdbtype[10], pdbatomname[10], pdbresidue[10];
    char pdbreschain;
    int pdbatomindex, pdbresidueseq, vec_counter = 0;
    float xpos, ypos, zpos, pdboccupy, pdbtemp, pdboffset;
    vec3 pos;
    FILE *inpars = fopen(file->filename, "r");

    enum ext_file filetype = in_file_ext(file->filename);

    switch (filetype) {
        case MOL_XYZ:
            filetype = MOL_XYZ;
            /* Skip first two lines of XYZ files. */
            (void)fgets(str, sizeof(str), inpars);
            (void)fgets(str, sizeof(str), inpars);
            break;
        case MOL_OBJ:
            filetype = MOL_OBJ;
            break;
        case MOL_PDB:
            filetype = MOL_PDB;
            break;
        default:
            pprint_err("Filetype of %s not recognized!\n", file->filename);
            break;
    }

    while (fgets (str, sizeof(str), inpars)) {
        /* Skip if needed */
        if (option->skip_model_vec && ++vec_counter < option->skip_model_vec) {
            continue;
        } else {
            vec_counter = 0;
        }
        if (!strstr(str, "#")) {
            switch (filetype) {
                case MOL_XYZ:
                    sscanf(str, " %s %f %f %f", atom, &xpos, &ypos, &zpos);
                    break;
                case MOL_PDB:
                    if(strncmp(str, "ATOM", 4)==0) {
                        sscanf(str, "%s %i %s %s %c %i %f %f %f %f %f %s %f\n",
                            pdbtype, &pdbatomindex, pdbatomname, pdbresidue,
                            &pdbreschain, &pdbresidueseq, &xpos, &ypos,
                            &zpos, &pdboccupy, &pdbtemp, atom, &pdboffset);
                    }
                    break;
                case MOL_OBJ:
                    if (!strncmp(str, "v ", 2)) {
                        sscanf(str, "v  %f %f %f", &xpos, &ypos, &zpos);
                    } else {
                        continue;
                    }
                    break;
                default:
                    continue;
                    break;
            }
            object[*i].atomnumber = return_atom_num(atom);
            object[*i].id = *i;
            pos = (vec3){xpos, ypos, zpos};
            rotate_vec(&pos, &file->rot);
            object[*i].pos = file->scale*pos + file->inf->pos;
            object[*i].vel = file->inf->vel;
            object[*i].ignore = file->inf->ignore;
            object[*i].atomnumber = file->inf->atomnumber;
            object[*i].radius = file->inf->radius ? file->inf->radius : 1;
            object[*i].mass = file->inf->mass ? file->inf->mass : 1.0;
            pprintf(PRI_SPAM, "%s atom %i here = {%lf, %lf, %lf}\n",
                    file->filename, *i,
                    object[*i].pos[0], object[*i].pos[1], object[*i].pos[2]);
            *i = *i + 1;
        }
    }
    fclose(inpars);
    return 0;
}
