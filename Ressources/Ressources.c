#include "Ressources.h"
#include "GrilleSDL.h"

// Macros pour les sprites
#define S_MUR                      200000

#define S_PACMAN_DROITE            200001
#define S_PACMAN_GAUCHE            200002
#define S_PACMAN_HAUT              200003
#define S_PACMAN_BAS               200004

#define S_FANTOME_ROUGE_DROITE     200005
#define S_FANTOME_ROUGE_GAUCHE     200006
#define S_FANTOME_ROUGE_HAUT       200007
#define S_FANTOME_ROUGE_BAS        200008
#define S_FANTOME_MAUVE_DROITE     200009
#define S_FANTOME_MAUVE_GAUCHE     200010
#define S_FANTOME_MAUVE_HAUT       200011
#define S_FANTOME_MAUVE_BAS        200012
#define S_FANTOME_VERT_DROITE      200013
#define S_FANTOME_VERT_GAUCHE      200014
#define S_FANTOME_VERT_HAUT        200015
#define S_FANTOME_VERT_BAS         200016
#define S_FANTOME_ORANGE_DROITE    200017
#define S_FANTOME_ORANGE_GAUCHE    200018
#define S_FANTOME_ORANGE_HAUT      200019
#define S_FANTOME_ORANGE_BAS       200020

#define S_FANTOME_COMESTIBLE       200021

#define S_PACGOM                   200022
#define S_SUPERPACGOM              200023
#define S_BONUS                    200024

#define S_CHIFFRE_0                300030
#define S_CHIFFRE_1                300031
#define S_CHIFFRE_2                300032
#define S_CHIFFRE_3                300033
#define S_CHIFFRE_4                300034
#define S_CHIFFRE_5                300035
#define S_CHIFFRE_6                300036
#define S_CHIFFRE_7                300037
#define S_CHIFFRE_8                300038
#define S_CHIFFRE_9                300039

#define S_GAMEOVER                 300040

void ChargementImages()
{
  // Definition de l'image de fond
  DessineImageFond("./images/fond.bmp");

  // Sprites
  AjouteSprite(S_MUR,"./images/mur.bmp");

  AjouteSprite(S_PACMAN_DROITE,"./images/pacman_droite.bmp");
  AjouteSprite(S_PACMAN_GAUCHE,"./images/pacman_gauche.bmp");
  AjouteSprite(S_PACMAN_HAUT,"./images/pacman_haut.bmp");
  AjouteSprite(S_PACMAN_BAS,"./images/pacman_bas.bmp");

  AjouteSprite(S_FANTOME_ROUGE_DROITE,"./images/fantome_rouge_droite.bmp");
  AjouteSprite(S_FANTOME_ROUGE_GAUCHE,"./images/fantome_rouge_gauche.bmp");
  AjouteSprite(S_FANTOME_ROUGE_HAUT,"./images/fantome_rouge_haut.bmp");
  AjouteSprite(S_FANTOME_ROUGE_BAS,"./images/fantome_rouge_bas.bmp");
  AjouteSprite(S_FANTOME_MAUVE_DROITE,"./images/fantome_mauve_droite.bmp");
  AjouteSprite(S_FANTOME_MAUVE_GAUCHE,"./images/fantome_mauve_gauche.bmp");
  AjouteSprite(S_FANTOME_MAUVE_HAUT,"./images/fantome_mauve_haut.bmp");
  AjouteSprite(S_FANTOME_MAUVE_BAS,"./images/fantome_mauve_bas.bmp");
  AjouteSprite(S_FANTOME_VERT_DROITE,"./images/fantome_vert_droite.bmp");
  AjouteSprite(S_FANTOME_VERT_GAUCHE,"./images/fantome_vert_gauche.bmp");
  AjouteSprite(S_FANTOME_VERT_HAUT,"./images/fantome_vert_haut.bmp");
  AjouteSprite(S_FANTOME_VERT_BAS,"./images/fantome_vert_bas.bmp");
  AjouteSprite(S_FANTOME_ORANGE_DROITE,"./images/fantome_orange_droite.bmp");
  AjouteSprite(S_FANTOME_ORANGE_GAUCHE,"./images/fantome_orange_gauche.bmp");
  AjouteSprite(S_FANTOME_ORANGE_HAUT,"./images/fantome_orange_haut.bmp");
  AjouteSprite(S_FANTOME_ORANGE_BAS,"./images/fantome_orange_bas.bmp");

  AjouteSprite(S_FANTOME_COMESTIBLE,"./images/fantome_comestible.bmp");
  AjouteSprite(S_PACGOM,"./images/pacgom.bmp");
  AjouteSprite(S_SUPERPACGOM,"./images/superpacgom.bmp");
  AjouteSprite(S_BONUS,"./images/bonus.bmp");

  AjouteSprite(S_CHIFFRE_0,"./images/Zero32.bmp");
  AjouteSprite(S_CHIFFRE_1,"./images/Un32.bmp");
  AjouteSprite(S_CHIFFRE_2,"./images/Deux32.bmp");
  AjouteSprite(S_CHIFFRE_3,"./images/Trois32.bmp");
  AjouteSprite(S_CHIFFRE_4,"./images/Quatre32.bmp");
  AjouteSprite(S_CHIFFRE_5,"./images/Cinq32.bmp");
  AjouteSprite(S_CHIFFRE_6,"./images/Six32.bmp");
  AjouteSprite(S_CHIFFRE_7,"./images/Sept32.bmp");
  AjouteSprite(S_CHIFFRE_8,"./images/Huit32.bmp");
  AjouteSprite(S_CHIFFRE_9,"./images/Neuf32.bmp");  

  AjouteSpriteAFondTransparent(S_GAMEOVER,"./images/GameOver.bmp",255,255,255);
}

// Fonctions Ressources
int OuvertureFenetreGraphique()
{
  if (OuvrirGrilleSDL(21,27,32,"!!! PAC-MAN !!!","./images/iconeFenetre256.bmp") < 0)
    return -1;
 
  ChargementImages();
  return 0;
}

int FermetureFenetreGraphique()
{
  FermerGrilleSDL();
  return 0;
}

void DessineMur(int l,int c)
{
  DessineSprite(l,c,S_MUR);
}

void DessinePacMan(int l,int c,int dir)
{
  switch(dir)
  {
    case DROITE  : DessineSprite(l,c,S_PACMAN_DROITE); break;
    case GAUCHE  : DessineSprite(l,c,S_PACMAN_GAUCHE); break;
    case HAUT    : DessineSprite(l,c,S_PACMAN_HAUT); break;
    case BAS     : DessineSprite(l,c,S_PACMAN_BAS); break;
    default     : DessineSprite(l,c,S_PACMAN_DROITE); break;    
  }
}

void DessineFantome(int l,int c,int couleur,int dir)
{
  switch(dir)
  {
    case DROITE  : if (couleur == ROUGE) DessineSprite(l,c,S_FANTOME_ROUGE_DROITE);
                   else if (couleur == MAUVE) DessineSprite(l,c,S_FANTOME_MAUVE_DROITE);
                   else if (couleur == VERT) DessineSprite(l,c,S_FANTOME_VERT_DROITE);
                   else if (couleur == ORANGE) DessineSprite(l,c,S_FANTOME_ORANGE_DROITE);
                   else DessineSprite(l,c,S_FANTOME_ROUGE_DROITE);
                   break;
    case GAUCHE  : if (couleur == ROUGE) DessineSprite(l,c,S_FANTOME_ROUGE_GAUCHE);
                   else if (couleur == MAUVE) DessineSprite(l,c,S_FANTOME_MAUVE_GAUCHE);
                   else if (couleur == VERT) DessineSprite(l,c,S_FANTOME_VERT_GAUCHE);
                   else if (couleur == ORANGE) DessineSprite(l,c,S_FANTOME_ORANGE_GAUCHE);
                   else DessineSprite(l,c,S_FANTOME_ROUGE_GAUCHE);
                   break;
    case HAUT    : if (couleur == ROUGE) DessineSprite(l,c,S_FANTOME_ROUGE_HAUT);
                   else if (couleur == MAUVE) DessineSprite(l,c,S_FANTOME_MAUVE_HAUT);
                   else if (couleur == VERT) DessineSprite(l,c,S_FANTOME_VERT_HAUT);
                   else if (couleur == ORANGE) DessineSprite(l,c,S_FANTOME_ORANGE_HAUT);
                   else DessineSprite(l,c,S_FANTOME_ROUGE_HAUT);
                   break;
    case BAS     : if (couleur == ROUGE) DessineSprite(l,c,S_FANTOME_ROUGE_BAS);
                   else if (couleur == MAUVE) DessineSprite(l,c,S_FANTOME_MAUVE_BAS);
                   else if (couleur == VERT) DessineSprite(l,c,S_FANTOME_VERT_BAS);
                   else if (couleur == ORANGE) DessineSprite(l,c,S_FANTOME_ORANGE_BAS);
                   else DessineSprite(l,c,S_FANTOME_ROUGE_BAS);
                   break;
    default     :  if (couleur == ROUGE) DessineSprite(l,c,S_FANTOME_ROUGE_DROITE);
                   else if (couleur == MAUVE) DessineSprite(l,c,S_FANTOME_MAUVE_DROITE);
                   else if (couleur == VERT) DessineSprite(l,c,S_FANTOME_VERT_DROITE);
                   else if (couleur == ORANGE) DessineSprite(l,c,S_FANTOME_ORANGE_DROITE);
                   else DessineSprite(l,c,S_FANTOME_ROUGE_DROITE);
                   break;
  }
}

void DessineFantomeComestible(int l,int c)
{
  DessineSprite(l,c,S_FANTOME_COMESTIBLE);
}

void DessinePacGom(int l,int c)
{
  DessineSprite(l,c,S_PACGOM);
}

void DessineSuperPacGom(int l,int c)
{
  DessineSprite(l,c,S_SUPERPACGOM);
}

void DessineBonus(int l,int c)
{
  DessineSprite(l,c,S_BONUS);
}

void DessineChiffre(int l,int c,int val)
{
  switch(val)
  {
    case 0 : DessineSprite(l,c,S_CHIFFRE_0); break;
    case 1 : DessineSprite(l,c,S_CHIFFRE_1); break;
    case 2 : DessineSprite(l,c,S_CHIFFRE_2); break;
    case 3 : DessineSprite(l,c,S_CHIFFRE_3); break;
    case 4 : DessineSprite(l,c,S_CHIFFRE_4); break;
    case 5 : DessineSprite(l,c,S_CHIFFRE_5); break;
    case 6 : DessineSprite(l,c,S_CHIFFRE_6); break;
    case 7 : DessineSprite(l,c,S_CHIFFRE_7); break;
    case 8 : DessineSprite(l,c,S_CHIFFRE_8); break;
    case 9 : DessineSprite(l,c,S_CHIFFRE_9); break;
    default : DessineSprite(l,c,S_CHIFFRE_0); break;    
  }
}

void DessineGameOver(int l,int c)
{
  DessineSprite(l,c,S_GAMEOVER);
}

