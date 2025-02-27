#ifndef RESSOURCES_H
#define RESSOURCES_H

// Macros pour les couleurs de fantome
#define ROUGE                     400000
#define MAUVE                     400001
#define VERT                      400002
#define ORANGE                    400003

// Macros pour les direction de Pac-Man et des fantomes
#define HAUT                      500000
#define BAS                       500001
#define GAUCHE                    500002
#define DROITE                    500003

int  OuvertureFenetreGraphique();
int  FermetureFenetreGraphique();
void DessineMur(int l,int c);
void DessinePacMan(int l,int c,int dir);
void DessineFantome(int l,int c,int couleur,int dir);
void DessineFantomeComestible(int l,int c);
void DessinePacGom(int l,int c);
void DessineSuperPacGom(int l,int c);
void DessineBonus(int l,int c);
void DessineChiffre(int l,int c,int val);
void DessineGameOver(int l,int c);

#endif
