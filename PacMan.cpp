#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <cerrno>
#include "GrilleSDL.h"
#include "Ressources.h"

// Dimensions de la grille de jeu
#define NB_LIGNE 21
#define NB_COLONNE 17

// Macros utilisees dans le tableau tab
#define VIDE         0
#define MUR          1
#define PACMAN       2
#define PACGOM       3
#define SUPERPACGOM  4
#define BONUS        5
#define FANTOME      6

// Autres macros
#define LENTREE 15
#define CENTREE 8

typedef struct
{
  int L;
  int C;
  int couleur;
  int cache;
} S_FANTOME;

typedef struct {
  int presence;
  pthread_t tid;
} S_CASE;

S_CASE tab[NB_LIGNE][NB_COLONNE];


pthread_t tidPacGom, tidPacMan, tidScore, tidEvent, tidBonus, tidCompteurFantomes, tidFantomes[8]; // Note : tidEvent peut etre dans le main
pthread_mutex_t mutexTab, mutexDelai, mutexNbPacGom, mutexScore, mutexLC, mutexNbFantomes; // Note : mutexLC utile ?
pthread_cond_t condNbPacGom, condScore, condNbFantomes;
pthread_key_t cle;
sigset_t maskPacMan; // Note : Est-ce bien que ce soit en global ?
bool MAJScore = true;
int L, C, dir; // Position et direction du PacMan
int nbPacGom, delai = 300, score = 0;
int nbRouge = 0, nbVert = 0, nbMauve = 0, nbOrange = 0;

void* threadPacGom(void *pParam);
void augmenterNiveau(int *niveau);
void* threadPacMan(void *pParam);
void calculerCoord(int direction, int *nouveauL, int *nouveauC);
void detecterPresenceProchaineCasePacMan(int l, int c);
void diminuerNbPacGom();
void augmenterScore(int augmentation);
void changerPositionPacMan(int nouveauL, int nouveauC, int direction, sigset_t mask);
void handlerSignaux(int signal);
void* threadScore(void *pParam);
void* threadBonus(void *pParam);
void* threadCompteurFantomes(void *pParam);
void creerFantome(S_FANTOME *structFantomes[8]);
void cleanupStructFantomes(void *pStructFantomes);
void* threadFantome(void *pParam);
void* threadEvent(void *pParam);
void DessineGrilleBase();
void Attente(int milli);
void setTab(int l, int c, int presence = VIDE, pthread_t tid = 0);
void messageInfo(const char *nomThread, const char *message);
void messageSucces(const char *nomThread, const char *message);
void messageErreur(const char* nomThread, const char *message);

// *********************** Initialisation du programme et lancement de chaque thread ***********************

int main(int argc,char* argv[])
{
  struct sigaction sigAct;
  char ok;
 
  srand((unsigned)time(NULL));

  // Ouverture de la fenetre graphique
  if (OuvertureFenetreGraphique() < 0)
  {
    messageErreur("MAIN", "Erreur de OuvrirGrilleSDL");
    fflush(stdout);
    exit(1);
  }
  messageSucces("MAIN", "Ouverture de la fenetre graphique reussie"); 

  // Creation de la grille de base
  DessineGrilleBase(); // Pas besoin du mutex, car aucun autre thread n'est lance

  // Exemple d'utilisation de GrilleSDL et Ressources --> code a supprimer
  // DessinePacMan(17, 7, GAUCHE);  // Attention !!! tab n'est pas modifie --> a vous de le faire !!!
  // DessineChiffre(14, 25, 9);
  // DessineFantome(5, 9, ROUGE, DROITE);
  // DessinePacGom(7, 4);
  // DessineSuperPacGom(9, 5);
  // DessineFantomeComestible(13, 15);
  // DessineBonus(5, 15);

  // Initialisation de mutexTab et de mutexDelai
  if (pthread_mutex_init(&mutexTab, NULL) != 0 || pthread_mutex_init(&mutexDelai, NULL) != 0 || pthread_mutex_init(&mutexNbPacGom, NULL) != 0 || pthread_mutex_init(&mutexScore, NULL) != 0 || pthread_mutex_init(&mutexLC, NULL) != 0 || pthread_mutex_init(&mutexNbFantomes, NULL) != 0)
  {
    messageErreur("MAIN", "Erreur de phtread_mutex_init");
    exit(1);
  }
  messageSucces("MAIN", "Initialisation de mutexTab, mutexDelai, mutexNbPacGom, mutexScore et mutexNbFantomes reussi");

  // Initialisation des variables de condition condNbPacGom, condScore et condNbFantomes
  pthread_cond_init(&condNbPacGom, NULL);
  pthread_cond_init(&condScore, NULL);
  pthread_cond_init(&condNbFantomes, NULL);

  // Masquage des signaux SIGINT, SIGHUP, SIGUSR1 et SIGUSR2
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  sigprocmask(SIG_SETMASK, &mask, NULL);

  // Creation de threadPacGom, threadPacman, threadScore et threadEvent
  pthread_create(&tidPacGom, NULL, threadPacGom, NULL);
  pthread_create(&tidScore, NULL, threadScore, NULL);
  pthread_create(&tidEvent, NULL, threadEvent, NULL);
  pthread_create(&tidPacMan, NULL, threadPacMan, NULL);
  pthread_create(&tidBonus, NULL, threadBonus, NULL);
  pthread_create(&tidCompteurFantomes, NULL, threadCompteurFantomes, NULL);
  
  pthread_join(tidPacMan, NULL);
  pthread_join(tidPacGom, NULL);
  pthread_join(tidScore, NULL);
  pthread_join(tidBonus, NULL);
  pthread_join(tidEvent, NULL);
  pthread_join(tidCompteurFantomes, NULL);

  messageInfo("MAIN", "Attente de 1500 millisecondes...");
  Attente(1500);
  // -------------------------------------------------------------------------
  
  // Fermeture de la fenetre
  messageInfo("MAIN", "Fermeture de la fenetre graphique...");
  FermetureFenetreGraphique();
  messageSucces("MAIN", "Fenetre graphique fermee");

  exit(0);
}

// *********************** Gestion du Pac-Man ***********************

void* threadPacGom(void *pParam)
{
  int l, c, niveauJeu = 1;
  int tabPosSuperPacGoms[4][2] = {{2, 1}, {2, 15}, {15, 1}, {15, 15}}; // Emplacement des Super Pac-Goms
  int tabPosVide[3][2] = {{15, 8}, {8, 8}, {9, 8}}; // Cases devant etre vides

  // Tant qu'on n'a pas atteint le niveau 10
  while (niveauJeu < 10)
  {
    // Afficher le niveau actuel
    DessineChiffre(14, 22, niveauJeu);

    pthread_mutex_lock(&mutexTab);
    pthread_mutex_lock(&mutexNbPacGom);

    nbPacGom = 0;
    
    // Remplir les cases vides avec des Pac-Goms
    for (l = 0; l < /*NB_LIGNE*/5; l++)
    {
      for (c = 0; c < /*NB_COLONNE*/5; c++)
      {
        if (tab[l][c].presence == 0)
        {
          setTab(l, c, PACGOM);
          DessinePacGom(l, c);
          nbPacGom++;
        }
      }
    }

    // Vider les cases devant etre vide
    for (int i = 0; i < (sizeof(tabPosVide) / sizeof(tabPosVide[0])); i++)
    {
      l = tabPosVide[i][0];
      c = tabPosVide[i][1];

      if (tab[l][c].presence == PACGOM)
      {
        setTab(l, c, VIDE);
        EffaceCarre(l, c);
        nbPacGom--;
      }
    }

    // Ajouter les super Pac-Goms
    for (int i = 0; i < (sizeof(tabPosSuperPacGoms) / sizeof(tabPosSuperPacGoms[0])); i++)
    {
      l = tabPosSuperPacGoms[i][0];
      c = tabPosSuperPacGoms[i][1];

      if (tab[l][c].presence != PACGOM) // Si on n'a pas rempli toute les cases vides avec des Pac-Goms
      {
        nbPacGom++;
      }

      setTab(l, c, SUPERPACGOM);
      DessineSuperPacGom(l,c);
    }

    pthread_mutex_unlock(&mutexTab);

    // Affiche le nombre total de Pac-Goms
    DessineChiffre(12, 22, nbPacGom / 100);
    DessineChiffre(12, 23, (nbPacGom % 100) / 10);
    DessineChiffre(12, 24, nbPacGom % 10);

    // Tant qu'il y a des Pac-Goms
    while (nbPacGom > 0)
    {
      pthread_cond_wait(&condNbPacGom, &mutexNbPacGom); // Attendre un changement de NbPacGom
      // Afficher le nombre de Pac-Goms restants.
      DessineChiffre(12, 22, nbPacGom / 100);
      DessineChiffre(12, 23, (nbPacGom % 100) / 10);
      DessineChiffre(12, 24, nbPacGom % 10);
    }
    pthread_mutex_unlock(&mutexNbPacGom);

    pthread_mutex_lock(&mutexTab);
    augmenterNiveau(&niveauJeu);
    pthread_mutex_unlock(&mutexTab);
  }

  // Note : Afficher le fait que le joueur a gagne
  
  pthread_exit(NULL);
}

void augmenterNiveau(int *niveau)
{
  // Augmenter le niveau du jeu de 1
  (*niveau)++;

  // Augmenter la vitess du Pac-Man par 2
  pthread_mutex_lock(&mutexDelai);
  delai /= 2;
  pthread_mutex_unlock(&mutexDelai);

  changerPositionPacMan(LENTREE, CENTREE, GAUCHE, maskPacMan);
}

// *********************** Gestion du Pac-Man ***********************

void* threadPacMan(void *pParam)
{
  int nouveauL, nouveauC, nouvelleDir, ancienneDir;
  int delaiLocal;
  dir = GAUCHE;
  L = LENTREE;
  C = CENTREE;

  // Demasquage des signaux SIGINT, SIGHUP, SIGUSR1 et SIGUSR2
  sigemptyset(&maskPacMan);
  sigaddset(&maskPacMan, SIGINT);
  sigaddset(&maskPacMan, SIGHUP);
  sigaddset(&maskPacMan, SIGUSR1);
  sigaddset(&maskPacMan, SIGUSR2);
  sigprocmask(SIG_UNBLOCK, &maskPacMan, NULL);

  // Armement des signaux SIGINT, SIGHUP, SIGUSR1 et SIUSR2
  struct sigaction sa;
  sa.sa_handler = handlerSignaux;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGHUP, &sa, NULL) == -1 || sigaction(SIGUSR1, &sa, NULL) == -1 || sigaction(SIGUSR2, &sa, NULL) == -1)
  {
    messageErreur("THREADPACMAN", "Erreur de sigaction. Des signaux pourraient ne pas fonctionner.\n");
  }

  pthread_mutex_lock(&mutexTab);
  // Placer le Pac-Man au point de départ
  setTab(L, C, PACMAN);
  DessinePacMan(L, C, GAUCHE);
  pthread_mutex_unlock(&mutexTab);

  // Boucled principale
  while (1)
  {
    // Recuperer la valeur de la variable delai
    pthread_mutex_lock(&mutexDelai);
    delaiLocal = delai; // Note : On la recupere pour ne pas bloquer tout le monde pendant qu'on attend
    pthread_mutex_unlock(&mutexDelai);

    // Attendre 300 millisecondes et puis la reception d'un signal
    sigprocmask(SIG_SETMASK, &maskPacMan, NULL);
    Attente(delaiLocal);
    sigprocmask(SIG_UNBLOCK, &maskPacMan, NULL);

    // Lire la nouvelle direction
    nouvelleDir = dir;

    pthread_mutex_lock(&mutexTab);
    // Calculer la nouvelle position selon la nouvelle direction
    calculerCoord(nouvelleDir, &nouveauL, &nouveauC);

    // Si le prochain emplacement n'est pas un mur
    if (tab[nouveauL][nouveauC].presence != MUR)
    {
      detecterPresenceProchaineCasePacMan(nouveauL, nouveauC);

      // On deplace le Pac-Man avec la nouvelle direction
      changerPositionPacMan(nouveauL, nouveauC, nouvelleDir, maskPacMan);
      ancienneDir = nouvelleDir;
    }
    else
    {
      // On tente de se deplacer avec l'ancienne direction
      calculerCoord(ancienneDir, &nouveauL, &nouveauC);

      // Si le prochain emplacement avec l'ancienne direction n'est pas un mur
      if (tab[nouveauL][nouveauC].presence != MUR)
      {
        detecterPresenceProchaineCasePacMan(nouveauL, nouveauC);

        // On deplace le Pac-Man avec l'ancienne direction
        changerPositionPacMan(nouveauL, nouveauC, ancienneDir, maskPacMan);
      }
    }
    pthread_mutex_unlock(&mutexTab);
  }
}

void calculerCoord(int direction, int *nouveauL, int *nouveauC)
{
  *nouveauL = L;
  *nouveauC = C;

  switch (direction) {
    case HAUT:
      (*nouveauL)--;
      break;

    case BAS:
      (*nouveauL)++;
      break;

    case GAUCHE:
      (*nouveauC)--;
      break;

    case DROITE:
      (*nouveauC)++;
      break;
  }
}

void detecterPresenceProchaineCasePacMan(int l, int c)
{
  switch (tab[l][c].presence)
  {
    case PACGOM:
      diminuerNbPacGom();
      augmenterScore(1);
      break;

    case SUPERPACGOM:
      diminuerNbPacGom();
      augmenterScore(5);
      break;

    case BONUS:
      augmenterScore(30);
      break;
  }
}

void diminuerNbPacGom()
{
  pthread_mutex_lock(&mutexNbPacGom);
  nbPacGom--;
  pthread_mutex_unlock(&mutexNbPacGom);
  pthread_cond_signal(&condNbPacGom);
}

void augmenterScore(int augmentation)
{
  pthread_mutex_lock(&mutexScore); // Note : oblige ?
  score += augmentation;
  MAJScore = true; // Indiquer que le score a ete mis a jour
  pthread_mutex_unlock(&mutexScore);
  pthread_cond_signal(&condScore);
}

// Sert a mettre à jour la position du PacMan et son affichage
void changerPositionPacMan(int nouveauL, int nouveauC, int direction, sigset_t mask)
{
  sigprocmask(SIG_SETMASK, &mask, NULL);
  setTab(L, C, VIDE);
  EffaceCarre(L, C);

  L = nouveauL;
  C = nouveauC;

  setTab(L, C, PACMAN);
  DessinePacMan(L, C, direction);
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

// Change la direction du Pac-Man a la reception d'un signal
void handlerSignaux(int signal)
{
  switch(signal)
  {
    case SIGINT:
      dir = GAUCHE;
      break;

    case SIGHUP:
      dir = DROITE;
      break;

    case SIGUSR1:
      dir = HAUT;
      break;

    case SIGUSR2:
      dir = BAS;
      break;
  }
}

// *********************** Gestion du score ***********************

void* threadScore(void *pParam)
{
  while (1)
  {
    pthread_mutex_lock(&mutexScore);
    // Tant qu'il y a des Pac-Goms
    while (MAJScore == false)
    {
      pthread_cond_wait(&condScore, &mutexScore); // Attendre un changement du score
    }
    pthread_mutex_unlock(&mutexScore);

    // Mettre a jour le score
    DessineChiffre(16, 22, score / 1000);
    DessineChiffre(16, 23, (score % 1000) / 100);
    DessineChiffre(16, 24, (score % 100) / 10);
    DessineChiffre(16, 25, score % 10);

    MAJScore = false;
  }
}

// *********************** Gestion des evenements ***********************

void* threadBonus(void *pParam)
{
  int l, c, lDepart, cDepart;
  bool bonusPlace;  // Flag pour vérifier si une case vide a été trouvée

  while (1)
  {
    Attente((rand() % 11 + 10) * 1000);

    // Generer un emplacement de depart aleatoire
    lDepart = rand() % NB_LIGNE;
    cDepart = rand() % NB_COLONNE;

    bonusPlace = false;

    pthread_mutex_lock(&mutexTab);
    // Rechercher une case vide a partir de l'emplacement de depart
    for (l = lDepart; l < NB_LIGNE && !bonusPlace; l++)
    {
      for (c = cDepart; c < NB_COLONNE && !bonusPlace; c++)
      {
        if (tab[l][c].presence == VIDE)
        {
          setTab(l, c, BONUS);
          DessineBonus(l, c);
          bonusPlace = true;
        }
      }
    }

    // Si on n'a pas encore place le bonus
    if (!bonusPlace)
    {
      // Rechercher une case vide a partir du debut du tableau jusqu'a l'emplacement initial
      for (l = 0; l < lDepart && !bonusPlace; l++)
      {
        for (c = 0; c < cDepart && !bonusPlace; c++)
        {
          if (tab[l][c].presence == VIDE)
          {
            setTab(l, c, BONUS);
            DessineBonus(l, c);
            bonusPlace = true;
          }
        }
      }
    }
    pthread_mutex_unlock(&mutexTab);

    // Attendre 10 secondes
    Attente(10000); // Note : Duree a verifier

    // Vu que la boucle a incremente les valeurs de l et c de 1 a la sortie, on les decremente de 1
    l--;
    c--;

    pthread_mutex_lock(&mutexTab);
    // Si le Bonus est encore la
    if (tab[l][c].presence == BONUS)
    {
      // On efface la Bonus
      setTab(l, c, VIDE);
      EffaceCarre(l, c);
    }
    pthread_mutex_unlock(&mutexTab);
  }
}

// *********************** Gestion des Fantomes ***********************

void* threadCompteurFantomes(void *pParam)
{
  S_FANTOME *structFantomes[8];

  int i, j, delaiLocal;

  // Allouer la memoire pour les structures Fantome
  for (i = 0; i < 8; i++)
  {
    structFantomes[i] = (S_FANTOME*)malloc(sizeof(S_FANTOME));

    // Si la memoire n'a pas ete alloue, liberer la memoire et arreter le thread
    if (structFantomes[i] == NULL)
    {
      for (j = 0; j < i; j++)
      {
          free(structFantomes[j]);
      }

      pthread_exit(NULL);
    }
  }

  // Mettre en place la fonction de liberation de la memoire
  // pthread_cleanup_push(cleanupStructFantomes, (void*)structFantomes);

  // Creation d'une cle pour des variables specifiques et ajout d'un destructeur
  pthread_key_create(&cle, free);

  // Boucle principale
  while (1)
  {
    pthread_mutex_lock(&mutexDelai);
    delaiLocal = delai; // Note : On la recupere pour ne pas bloquer tout le monde pendant l'attente
    pthread_mutex_unlock(&mutexDelai);

    Attente((delaiLocal * 5) / 3);

    pthread_mutex_lock(&mutexNbFantomes);
    // Tant qu'il y a tous les Fantomes
    while (nbRouge == 2 && nbVert == 2 && nbMauve == 2 && nbOrange == 2)
    {
        pthread_cond_wait(&condNbFantomes, &mutexNbFantomes);
    }
    pthread_mutex_unlock(&mutexNbFantomes);

    // Note : Faire un pthread_cond_signal quand un Fantome est tue

    creerFantome(structFantomes);
  }

  // Jamais atteint
  // pthread_cleanup_pop(1);
}

void creerFantome(S_FANTOME *structFantomes[8])
{
  int i;

  pthread_mutex_lock(&mutexNbFantomes);
  // Trouver le Fantome a creer
  if (nbRouge < 2)
  {
    if (nbRouge == 0) i = 0;
    else i = 1;

    structFantomes[i]->couleur = ROUGE;
    nbRouge++;
  }
  else if (nbVert < 2)
  {
    if (nbVert == 0) i = 2;
    else i = 3;
    
    structFantomes[i]->couleur = VERT;
    nbVert++;
  }
  else if (nbMauve < 2)
  {
    if (nbMauve == 0) i = 4;
    else i = 5;

    structFantomes[i]->couleur = MAUVE;
    nbMauve++;
  }
  else if (nbOrange < 2)
  {
    if (nbOrange == 0) i = 6;
    else i = 7;
    
    structFantomes[i]->couleur = ORANGE;
    nbOrange++;
  }
  else
  {
    return; // Pas necessaire
  }

  structFantomes[i]->L = 9;
  structFantomes[i]->C = 8;
  structFantomes[i]->cache = 0;

  // Creer un threadFantome
  if (pthread_create(&tidFantomes[i], NULL, threadFantome, (void*)structFantomes[i]) != 0)
  {
    messageErreur("THREADCOMPTEURFANTOMES", "Erreur de pthread_create");
    // Note : Mettre a jour le nombre de Fantomes
  }
  pthread_mutex_unlock(&mutexNbFantomes);
}

// Non necessaire, car on a pthread_key_create(&cle, free);
// void cleanupStructFantomes(void *pStructFantomes)
// {
//   printf("CleanupStructFantomes active\n");
//   fflush(stdout);
//   S_FANTOME **ppStructFantomes = (S_FANTOME **)pStructFantomes;

//   int i;

//   for (i = 0; i < 8; i++)
//   {
//     free(ppStructFantomes[i]);
//   }
//   printf("Fin de CleanupStructFantomes\n");
//   fflush(stdout);
// }

// *********************** Gestion d'un Fantome ***********************

void* threadFantome(void *pParam)
{
  S_FANTOME *pStructFantome = (S_FANTOME *)pParam;
  int *l = &(pStructFantome->L), *c = &(pStructFantome->C), *couleur = &(pStructFantome->couleur), *cache = &(pStructFantome->cache);
  int dir = HAUT; // Note : Attention a la variable dir globale ?
  int nouveauL, nouveauC, nouvelleDir = dir;
  int delaiLocal;
  bool caseDepartOccupee = true, caseSuivanteTrouvee;

  pthread_setspecific(cle, pStructFantome);

  // Tant qu'il y a un Fantome sur la case de depart
  // Necessaire ?
  while (caseDepartOccupee)
  {
    pthread_mutex_lock(&mutexTab);
    // Si la case n'est plus occupee
    if (tab[*l][*c].presence != FANTOME)
    {
      caseDepartOccupee = false; // Signaler que la case n'est plus occupee
    }
    pthread_mutex_unlock(&mutexTab);

    // Si la case est occupee
    if (caseDepartOccupee == true)
    {
      pthread_mutex_lock(&mutexDelai);
      delaiLocal = (delai * 5) / 3;
      pthread_mutex_unlock(&mutexDelai);

      Attente(delaiLocal); // On attend la duree d'un delai
    }
  }

  // Afficher le Fantome
  pthread_mutex_lock(&mutexTab);
  setTab(*l, *c, FANTOME, pthread_self());
  DessineFantome(*l, *c, dir, *couleur);
  pthread_mutex_unlock(&mutexTab);

  while (1)
  {
    caseSuivanteTrouvee = false;

    // Attente d'un delai
    pthread_mutex_lock(&mutexDelai);
    delaiLocal = (delai * 5) / 3;
    pthread_mutex_unlock(&mutexDelai);

    Attente(delaiLocal);

    pthread_mutex_lock(&mutexTab);
    // Tant qu'on n'a pas trouve la case suivante
    while (!caseSuivanteTrouvee)
    {
      nouveauL = *l;
      nouveauC = *c;

      switch (nouvelleDir)
      {
        case HAUT:
          nouveauL--;
          break;

        case BAS:
          nouveauL++;
          break;

        case GAUCHE:
          nouveauC--;
          break;

        case DROITE:
          nouveauC++;
          break;
      }

      // S'il n'y a pas de mur ou de fantome dans cette direction
      if (tab[nouveauL][nouveauC].presence != MUR && tab[nouveauL][nouveauC].presence != FANTOME)
      {
        // Restaurer l'ancienne case
        switch (*cache)
        {
          case VIDE:
            EffaceCarre(*l, *c);
            break;

          case PACGOM:
            DessinePacGom(*l, *c);
            break;

          case SUPERPACGOM:
            DessineSuperPacGom(*l, *c);
            break;

          case BONUS:
            DessineBonus(*l, *c);
            break;
        }

        // Si la prochaine case est le Pac-Man
        if (tab[nouveauL][nouveauC].presence == PACMAN)
        {
          pthread_cancel(tidPacMan);
        }

        // Avancer le Fantome
        *l = nouveauL;
        *c = nouveauC;
        dir = nouvelleDir;

        setTab(*l, *c, FANTOME, pthread_self());
        DessineFantome(*l, *c, dir, *couleur);

        caseSuivanteTrouvee = true;
      }
      else
      {
        // Generer une direction aleatoire
        nouvelleDir = (rand() % 4) + 500000; // Note : Est-ce bien d'utiliser un nombre magique ?
      }
    }

    pthread_mutex_unlock(&mutexTab);
  }
}

// *********************** Gestion des evenements ***********************

void* threadEvent(void *pParam)
{
  EVENT_GRILLE_SDL event;

  int quitter = 0;

  while(!quitter)
  {
    event = ReadEvent();

    if (event.type == CROIX) quitter = 1;

    if (event.type == CLAVIER)
    {
      switch(event.touche)
      {
        case 'q' :
          quitter = 1;
          break;

        case KEY_UP :
          printf("Fleche haut !\n");
          pthread_kill(tidPacMan, SIGUSR1);
          break;

        case KEY_DOWN :
          printf("Fleche bas !\n");
          pthread_kill(tidPacMan, SIGUSR2);
          break;

        case KEY_RIGHT :
          printf("Fleche droite !\n");
          pthread_kill(tidPacMan, SIGHUP);
          break;

        case KEY_LEFT :
          printf("Fleche gauche !\n");
          pthread_kill(tidPacMan, SIGINT);
          break;
      }
    }
  }

  for (int i = 0; i < 8; i++)
  {
    if (pthread_cancel(tidFantomes[i]) != 0)
    {
      messageErreur("EVENT", "Un thread Fantome n'a pas pu etre annule");
    }
    else
    {
      messageSucces("EVENT", "Le thread Fantome a ete annule");
      pthread_join(tidFantomes[i], NULL);
    }
  }

  // Annulation de threadPacMan, threadPacGom et threadScore
  if (pthread_cancel(tidPacMan) != 0 || pthread_cancel(tidPacGom) != 0 || pthread_cancel(tidScore) != 0 || pthread_cancel(tidBonus) != 0 || pthread_cancel(tidCompteurFantomes) != 0)
  {
    messageErreur("EVENT", "Tous les threads n'ont pas pu etre annule");
    // Note : Attention ! Si le Pac-Man est tue, tidPacMan n'est plus correct.
  }
  else
  {
    messageSucces("EVENT", "Tous les threads ont ete annule");
  }

  pthread_exit(NULL);
}

// *********************** Fonction d'attente ***********************

void Attente(int milli) {
  struct timespec del;
  del.tv_sec = milli/1000;
  del.tv_nsec = (milli%1000)*1000000;
  nanosleep(&del, NULL);
}

// *********************** Fonction pour placer les composants dans la table ***********************

void setTab(int l, int c, int presence, pthread_t tid) {
  tab[l][c].presence = presence;
  tab[l][c].tid = tid;
}

// *********************** Fonction pour initialiser la table ***********************

void DessineGrilleBase() {
  int t[NB_LIGNE][NB_COLONNE]
    = { {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
        {1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0,1},
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,0,1,1,0,1,0,1,1,1,0,1,0,1,1,0,1},
        {1,0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,1},
        {1,1,1,1,0,1,1,0,1,0,1,1,0,1,1,1,1},
        {1,1,1,1,0,1,0,0,0,0,0,1,0,1,1,1,1},
        {1,1,1,1,0,1,0,1,0,1,0,1,0,1,1,1,1},
        {0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0},
        {1,1,1,1,0,1,0,1,1,1,0,1,0,1,1,1,1},
        {1,1,1,1,0,1,0,0,0,0,0,1,0,1,1,1,1},
        {1,1,1,1,0,1,0,1,1,1,0,1,0,1,1,1,1},
        {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1},
        {1,0,1,1,0,1,1,0,1,0,1,1,0,1,1,0,1},
        {1,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,1},
        {1,1,0,1,0,1,0,1,1,1,0,1,0,1,0,1,1},
        {1,0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,1},
        {1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,0,1},
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}};

  for (int l = 0 ; l < NB_LIGNE ; l++)
    for (int c = 0 ; c < NB_COLONNE ; c++) {
      if (t[l][c] == VIDE) {
        setTab(l, c);
        EffaceCarre(l, c);
      }
      if (t[l][c] == MUR) {
        setTab(l, c, MUR); 
        DessineMur(l, c);
      }
    }
}

// *********************** Fonctions d'affichage ***********************

void messageInfo(const char *nomThread, const char *message)
{
  printf("(%s %lu) (INFO) %s\n", nomThread, pthread_self(), message);
  fflush(stdout);
}

void messageSucces(const char *nomThread, const char *message)
{
  printf("\033[32m(%s %lu) (SUCCES) %s\033[0m\n", nomThread, pthread_self(), message);
  fflush(stdout);
}

void messageErreur(const char* nomThread, const char *message)
{
  fprintf(stderr, "\033[31m(%s %lu) (ERREUR) %s\033[0m\n", nomThread, pthread_self(), message);
  fflush(stderr);

  if (errno != 0)
  {
    perror("Message ");
    errno = 0;
  }
}
