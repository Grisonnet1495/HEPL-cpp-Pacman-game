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


pthread_t tidPacGom, tidPacMan = 0, tidScore, tidEvent, tidBonus, tidCompteurFantomes, tidVies, tidTimeOut = 0, tidFantomes[8] = { 0 }; // Note : tidEvent peut etre dans le main
pthread_mutex_t mutexTab, mutexDelai, mutexNbPacGom, mutexScore, mutexNbFantomes, mutexMode;
pthread_cond_t condNbPacGom, condScore, condNbFantomes, condMode;
pthread_key_t cle;
sigset_t maskPacMan; // Note : Est-ce bien que ce soit en global ?
bool MAJScore = true;
bool continuerJeu = true;
int L, C, dir; // Position et direction du PacMan
int nbPacGom, delai = 300, score = 0;
int nbRouge = 0, nbVert = 0, nbMauve = 0, nbOrange = 0;
int mode = 1;

void* threadPacGom(void *pParam);
void initialiserPacGoms();
void afficherNbPacGoms();
void augmenterNiveau(int *niveau);
void* threadPacMan(void *pParam);
void placerPacManEtAttente();
void calculerCoord(int direction, int *nouveauL, int *nouveauC);
void detecterProchaineCasePacMan(int l, int c);
void diminuerNbPacGom();
void augmenterScore(int augmentation);
void changerPositionPacMan(int nouveauL, int nouveauC, int direction, sigset_t mask);
void handlerSignaux(int signal);
void* threadScore(void *pParam);
void* threadBonus(void *pParam);
void* threadCompteurFantomes(void *pParam);
bool allouerStructFantome(S_FANTOME *structFantomes[8], int i, int couleur);
void cleanupStructFantomes(void *pStructFantomes);
void* threadFantome(void *pParam);
void restaurerAncienneCase(int *l, int *c, int *cache);
void detecterProchaineCaseFantome(int nouveauL, int nouveauC, int *cache);
void handlerSIGCHLD(int signal);
void cleanupFantome(void *pParam);
void* threadTimeOut(void *pParam);
void handlerSIGQUIT(int signal);
void handlerSIGALRM(int signal);
void* threadVies(void *pParam);
void cleanupMutexTab(void *p);
void* threadEvent(void *pParam);
void annulerThreads();
void annulerAutreThreads();
void annulerThreadsFantomes();
void DessineGrilleBase();
void Attente(int milli);
void setTab(int l, int c, int presence = VIDE, pthread_t tid = 0);
void messageInfo(const char *nomThread, const char *message);
void messageSucces(const char *nomThread, const char *message);
void messageErreur(const char* nomThread, const char *message);

// *********************** Initialisation du programme et lancement de chaque thread ***********************

int main(int argc,char* argv[])
{
  // struct sigaction sigAct; // Note : A utiliser ?
  // char ok; // Note : A utiliser ?
 
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

  // Initialisation des mutex
  if (pthread_mutex_init(&mutexTab, NULL) != 0 || pthread_mutex_init(&mutexDelai, NULL) != 0 || pthread_mutex_init(&mutexNbPacGom, NULL) != 0 || pthread_mutex_init(&mutexScore, NULL) != 0 || pthread_mutex_init(&mutexNbFantomes, NULL) != 0 || pthread_mutex_init(&mutexMode, NULL) != 0)
  {
    messageErreur("MAIN", "Erreur de phtread_mutex_init");
    exit(1);
  }
  messageSucces("MAIN", "Initialisation de mutexTab, mutexDelai, mutexNbPacGom, mutexScore et mutexNbFantomes reussie");

  // Initialisation des variables de condition
  if (pthread_cond_init(&condNbPacGom, NULL) != 0 || pthread_cond_init(&condScore, NULL) != 0 || pthread_cond_init(&condNbFantomes, NULL) != 0 || pthread_cond_init(&condMode, NULL) != 0)
  {
    messageErreur("MAIN", "Erreur de phtread_cond_init");
    exit(1);
  }
  messageSucces("MAIN", "Initialisation de condNbPacGom, condScore et condNbFantomes reussie");

  // Masquage des signaux SIGINT, SIGHUP, SIGUSR1, SIGUSR2, SIGALRM et SIGQUIT
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  sigaddset(&mask, SIGALRM);
  sigaddset(&mask, SIGQUIT);
  sigaddset(&mask, SIGCHLD);
  // sigfillset(&mask); // Note : correct ?
  // sigdelset(&mask, SIGINT);
  sigprocmask(SIG_SETMASK, &mask, NULL);

  // Creation d'une cle pour des variables specifiques
  pthread_key_create(&cle, NULL);

  // Creation de threadPacGom, threadScore, threadPacman, threadEvent, threadBonus, threadCompteurFantomes et threadVies
  pthread_create(&tidPacGom, NULL, threadPacGom, NULL);
  pthread_create(&tidScore, NULL, threadScore, NULL);
  pthread_create(&tidEvent, NULL, threadEvent, NULL);
  pthread_create(&tidBonus, NULL, threadBonus, NULL);
  pthread_create(&tidCompteurFantomes, NULL, threadCompteurFantomes, NULL);
  pthread_create(&tidVies, NULL, threadVies, NULL);
  
  // Attente de threadEvent
  pthread_join(tidEvent, NULL);

  messageInfo("MAIN", "Attente de 1500 millisecondes...");
  Attente(1500);
  // -------------------------------------------------------------------------
  
  // Fermeture de la fenetre
  messageInfo("MAIN", "Fermeture de la fenetre graphique...");
  FermetureFenetreGraphique();
  messageSucces("MAIN", "Fenetre graphique fermee");

  // Annuler tous les threads principaux
  annulerThreads();
  // Annuler tous les threadFantome
  annulerThreadsFantomes();
  // Annuler les autres threads
  annulerAutreThreads();

  // Suppression de la cle pour les variables specifiques
  pthread_key_delete(cle);

  // Suppression des mutex
  pthread_mutex_destroy(&mutexTab);
  pthread_mutex_destroy(&mutexDelai);
  pthread_mutex_destroy(&mutexNbPacGom);
  pthread_mutex_destroy(&mutexScore);
  pthread_mutex_destroy(&mutexNbFantomes);
  pthread_mutex_destroy(&mutexMode);
  messageSucces("MAIN", "mutexTab, mutexDelai, mutexNbPacGom, mutexScore, mutexLC et mutexNbFantomes supprimes");

  // Suppresion des variables de condition
  pthread_cond_destroy(&condNbPacGom);
  pthread_cond_destroy(&condScore);
  pthread_cond_destroy(&condNbFantomes);
  pthread_cond_destroy(&condMode);
  messageSucces("MAIN", "condNbPacGom, condScore et condNbFantomes supprimees");

  exit(0);
}

// *********************** Gestion du Pac-Man ***********************

void* threadPacGom(void *pParam)
{
  messageSucces("PACGOM", "Thread cree");

  int niveauJeu = 1;

  // Tant qu'on n'a pas atteint le niveau 10
  while (niveauJeu < 10)
  {
    // Afficher le niveau actuel
    DessineChiffre(14, 22, niveauJeu);

    // Initialiser les Pac-Goms
    pthread_mutex_lock(&mutexNbPacGom);
    initialiserPacGoms();

    // Affiche le nombre total de Pac-Goms
    afficherNbPacGoms();

    // Tant qu'il y a des Pac-Goms
    while (nbPacGom > 0)
    {
      pthread_cond_wait(&condNbPacGom, &mutexNbPacGom); // Attendre un changement de NbPacGom
      // Afficher le nombre de Pac-Goms restants.
      afficherNbPacGoms();
    }
    pthread_mutex_unlock(&mutexNbPacGom);

    pthread_testcancel();

    printf("Changmement de niveau\n");
    pthread_mutex_lock(&mutexTab);
    augmenterNiveau(&niveauJeu);
    pthread_mutex_unlock(&mutexTab);
  }

  // Note : Afficher le fait que le joueur a gagne
  
  tidPacGom = 0;
  messageInfo("PACGOM", "Le thread se termine");
  pthread_exit(NULL);
}

void initialiserPacGoms()
{
  int l, c;
  int tabPosSuperPacGoms[4][2] = {{2, 1}, {2, 15}, {15, 1}, {15, 15}}; // Emplacement des Super Pac-Goms
  int tabPosVide[3][2] = {{15, 8}, {8, 8}, {9, 8}}; // Cases devant etre vides

  pthread_mutex_lock(&mutexTab);

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
  for (unsigned long int i = 0; i < (sizeof(tabPosVide) / sizeof(tabPosVide[0])); i++)
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
  for (unsigned long int i = 0; i < (sizeof(tabPosSuperPacGoms) / sizeof(tabPosSuperPacGoms[0])); i++)
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
}

void afficherNbPacGoms()
{
  DessineChiffre(12, 22, nbPacGom / 100);
  DessineChiffre(12, 23, (nbPacGom % 100) / 10);
  DessineChiffre(12, 24, nbPacGom % 10);
}

void augmenterNiveau(int *niveau)
{
  // Augmenter le niveau du jeu de 1
  (*niveau)++;

  // Augmenter la vitess du Pac-Man par 2
  pthread_mutex_lock(&mutexDelai);
  delai /= 2;
  pthread_mutex_unlock(&mutexDelai);

  // Annuler tous les threadFantome pour qu'ils reviennent au depart
  annulerThreadsFantomes();

  // Efface les Fantômes qui sont affiches
  for (int l = 0; l < NB_LIGNE; l++)
  {
    for (int c = 0; c < NB_COLONNE; c++)
    {
      if (tab[l][c].presence == FANTOME)
      {
        EffaceCarre(l, c);
        setTab(l, c, VIDE);
      }
    }
  }

  // Si on est dans le mode Fantomes comestible, on revient dans le mode normal
  printf("tidTimeout = %lu\n", tidTimeOut);
  if (tidTimeOut != 0)
  {
    pthread_kill(tidTimeOut, SIGQUIT);
    tidTimeOut = 0;
    printf("Signal SIGQUIT envoye\n");
    alarm(0);
    printf("alarm(0) fait\n");
    pthread_mutex_lock(&mutexMode);
    mode = 1;
    pthread_mutex_unlock(&mutexMode);
    printf("mode = 1\n");
    pthread_cond_signal(&condMode);
    printf("pthread_cond_signal envoye\n");
  }
  printf("tidTimeOut verifie\n");

  // Efface l'emplacement courant du Pac-Man
  EffaceCarre(L, C);
  setTab(L, C, VIDE);
  // Remet le Pac-Man a la position de depart
  placerPacManEtAttente();

  changerPositionPacMan(LENTREE, CENTREE, GAUCHE, maskPacMan);
}

// *********************** Gestion du Pac-Man ***********************

void* threadPacMan(void *pParam)
{
  messageSucces("PACMAN", "Thread cree");

  int nouveauL, nouveauC, nouvelleDir, ancienneDir;
  int delaiLocal;

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
    messageErreur("PACMAN", "Erreur de sigaction. Des signaux pourraient ne pas fonctionner.\n");
  }

  sigprocmask(SIG_BLOCK, &maskPacMan, NULL);
  pthread_mutex_lock(&mutexTab);
  placerPacManEtAttente();
  pthread_mutex_unlock(&mutexTab);
  sigprocmask(SIG_UNBLOCK, &maskPacMan, NULL);

  // Boucled principale
  while (continuerJeu)
  {
    pthread_testcancel();

    // Recuperer la valeur de la variable delai
    pthread_mutex_lock(&mutexDelai);
    delaiLocal = delai; // Note : On la recupere pour ne pas bloquer tout le monde pendant qu'on attend
    pthread_mutex_unlock(&mutexDelai);

    // Attendre 300 millisecondes et puis la reception d'un signal
    sigprocmask(SIG_BLOCK, &maskPacMan, NULL);
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
      detecterProchaineCasePacMan(nouveauL, nouveauC);

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
        detecterProchaineCasePacMan(nouveauL, nouveauC);

        // On deplace le Pac-Man avec l'ancienne direction
        changerPositionPacMan(nouveauL, nouveauC, ancienneDir, maskPacMan);
      }
    }
    pthread_mutex_unlock(&mutexTab);
  }

  tidPacMan = 0;
  messageInfo("PACMAN", "Thread termine");
  pthread_exit(NULL);
}

void placerPacManEtAttente()
{
  dir = GAUCHE;
  L = LENTREE;
  C = CENTREE;

  // Placer le Pac-Man au point de départ
  setTab(L, C, PACMAN);

  // Faire clignoter le PacMan
  for (int i = 0; i < 3; i++)
  {
    DessinePacMan(L, C, GAUCHE);
    Attente(500);
    EffaceCarre(L, C);
    Attente(500);
  }
  
  DessinePacMan(L, C, GAUCHE);
  Attente(500);
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

void detecterProchaineCasePacMan(int l, int c)
{
  int *nbSecondes = (int *)malloc(sizeof(int));

  switch (tab[l][c].presence)
  {
    case PACGOM:
      diminuerNbPacGom();
      augmenterScore(1);
      break;

    case SUPERPACGOM:
      diminuerNbPacGom();
      augmenterScore(5);
      // Changer de mode
      pthread_mutex_lock(&mutexMode);
      mode = 2;
      pthread_mutex_unlock(&mutexMode);
      // Creer un threadTimeOut
      *nbSecondes = 0;

      if (tidTimeOut)
      {
        pthread_kill(tidTimeOut, SIGQUIT);
        tidTimeOut = 0;
        *nbSecondes = alarm(0);
      }
      pthread_create(&tidTimeOut, NULL, threadTimeOut, nbSecondes);
      pthread_detach(tidTimeOut);
      break;

    case BONUS:
      augmenterScore(30);
      break;

    case FANTOME:
      pthread_mutex_lock(&mutexMode);
      // Si on est dans le mode normal
      if (mode == 1)
      {
        pthread_mutex_unlock(&mutexMode);
        pthread_mutex_unlock(&mutexTab);

        // Tuer le Pac-Man
        EffaceCarre(L, C);
        setTab(L, C, VIDE);

        messageInfo("PACMAN", "Le Pac-Man a ete mange par un Fantome");
        pthread_exit(NULL);
      }
      else // Si on est dans le mode Fantomes comestibles
      {
        pthread_mutex_unlock(&mutexMode);

        // Tuer le Fantome
        pthread_kill(tab[l][c].tid, SIGCHLD);

        for (int i = 0; i < 8; i++)
        {
          if (tidFantomes[i] == tab[l][c].tid)
          {
            tidFantomes[i] = 0;
          }
        }
      }
      
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
  pthread_mutex_lock(&mutexScore);
  score += augmentation;
  MAJScore = true; // Indiquer que le score a ete mis a jour
  pthread_mutex_unlock(&mutexScore);
  pthread_cond_signal(&condScore);
}

// Sert a mettre à jour la position du PacMan et son affichage
void changerPositionPacMan(int nouveauL, int nouveauC, int direction, sigset_t mask)
{
  sigprocmask(SIG_BLOCK, &mask, NULL);
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
  messageSucces("SCORE", "Thread cree");

  while (continuerJeu)
  {
    pthread_testcancel();

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

  tidScore = 0;
  messageSucces("SCORE", "Thread termine");
  pthread_exit(NULL);
}

// *********************** Gestion des evenements ***********************

void* threadBonus(void *pParam)
{
  messageSucces("BONUS", "Thread cree");
  
  int l, c, lDepart, cDepart;
  bool bonusPlace;  // Flag pour vérifier si une case vide a été trouvée

  while (continuerJeu)
  {
    pthread_testcancel();

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

  tidBonus = 0;
  messageInfo("BONUS", "Thread termine");
  pthread_exit(NULL);
}

// *********************** Gestion des Fantomes ***********************

void* threadCompteurFantomes(void *pParam)
{
  messageSucces("COMPTEURFANTOMES", "Thread cree");
  
  S_FANTOME *structFantomes[8];

  int delaiLocal;
  bool creerFantomePossible = false;

  // Boucle principale
  while (continuerJeu)
  {
    pthread_testcancel();

    pthread_mutex_lock(&mutexDelai);
    delaiLocal = (delai * 5) / 3; // Note : On la recupere pour ne pas bloquer tout le monde pendant l'attente
    pthread_mutex_unlock(&mutexDelai);

    Attente(delaiLocal);

    pthread_mutex_lock(&mutexNbFantomes);
    // Tant qu'il y a tous les Fantomes
    while (nbRouge == 2 && nbVert == 2 && nbMauve == 2 && nbOrange == 2)
    {
      pthread_cond_wait(&condNbFantomes, &mutexNbFantomes);
    }
    pthread_mutex_unlock(&mutexNbFantomes);

    pthread_testcancel();

    pthread_mutex_lock(&mutexMode);
    // Tant qu'on n'est dans le mode Fantomes comestibles
    while (mode == 2)
    {
      pthread_cond_wait(&condMode, &mutexMode);
    }
    pthread_mutex_unlock(&mutexMode);

    pthread_testcancel();

    // creerFantome(structFantomes);
    int i;

    pthread_mutex_lock(&mutexNbFantomes);
    // Trouver le Fantome a creer
    if (nbRouge < 2)
    {
      if (nbRouge == 0) i = 0;
      else i = 1;

      creerFantomePossible = allouerStructFantome(structFantomes, i, ROUGE);
      nbRouge++;
    }
    else if (nbVert < 2)
    {
      if (nbVert == 0) i = 2;
      else i = 3;
      
      creerFantomePossible = allouerStructFantome(structFantomes, i, VERT);
      nbVert++;
    }
    else if (nbMauve < 2)
    {
      if (nbMauve == 0) i = 4;
      else i = 5;

      creerFantomePossible = allouerStructFantome(structFantomes, i, MAUVE);
      nbMauve++;
    }
    else if (nbOrange < 2)
    {
      if (nbOrange == 0) i = 6;
      else i = 7;
      
      creerFantomePossible = allouerStructFantome(structFantomes, i, ORANGE);
      nbOrange++;
    }
    else
    {
      creerFantomePossible = false;
    }
    pthread_mutex_unlock(&mutexNbFantomes);

    // Creer un threadFantome
    if (creerFantomePossible)
    {
      if (pthread_create(&tidFantomes[i], NULL, threadFantome, (void*)structFantomes[i]) != 0)
      {
        messageErreur("COMPTEURFANTOMES", "Erreur de pthread_create");
      }
    }
  }

  tidCompteurFantomes = 0;
  messageInfo("COMPTEURFANTOMES", "Thread termine");
  pthread_exit(NULL);
}

bool allouerStructFantome(S_FANTOME *structFantomes[8], int i, int couleur)
{
  int j;

  structFantomes[i] = (S_FANTOME*)malloc(sizeof(S_FANTOME));

  // Si la memoire n'a pas ete alloue, liberer la memoire et arreter le thread
  if (structFantomes[i] == NULL)
  {
    for (j = 0; j < i; j++)
    {
        free(structFantomes[j]);
    }

    return false;
  }

  structFantomes[i]->couleur = couleur;
  structFantomes[i]->L = 9;
  structFantomes[i]->C = 8;
  structFantomes[i]->cache = VIDE;

  return true;
}

// *********************** Gestion d'un Fantome ***********************

void* threadFantome(void *pParam)
{
  messageSucces("FANTOME", "Thread cree");
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); // Empeche que le thread ne puisse pas etre annule s'il est bloque sur un mutex.
  
  S_FANTOME *pStructFantome = (S_FANTOME *)pParam;
  int *l = &(pStructFantome->L), *c = &(pStructFantome->C), *couleur = &(pStructFantome->couleur), *cache = &(pStructFantome->cache);
  int dir = HAUT; // Note : Attention a la variable dir globale ?
  int nouveauL, nouveauC, nouvelleDir = dir;
  int delaiLocal;
  int tentative; // Necessaire pour ne pas bloquer les autres threads si un Fantome est bloque
  bool caseDepartOccupee = true, caseSuivanteTrouvee;

  pthread_setspecific(cle, pStructFantome);

  pthread_cleanup_push(cleanupFantome, 0);

  // Demasquage du signal SIGCHLD
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  pthread_sigmask(SIG_UNBLOCK, &mask, NULL); // Note : On pourrait mettre SIG_SETMASK

  // Armement des signaux SIGCHLD
  struct sigaction sa;
  sa.sa_handler = handlerSIGCHLD;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGCHLD, &sa, NULL) == -1)
  {
    messageErreur("FANTOME", "Erreur de sigaction. Le signal SIGCHLD ne fonctionnera pas.\n");
  }

  // Tant qu'il y a un Fantome sur la case de depart
  while (caseDepartOccupee)
  {
    pthread_testcancel();

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
  DessineFantome(*l, *c, *couleur, dir);
  pthread_mutex_unlock(&mutexTab);

  while (continuerJeu)
  {
    pthread_testcancel();

    caseSuivanteTrouvee = false;
    tentative = 0;

    // Attente d'un delai
    pthread_mutex_lock(&mutexDelai);
    delaiLocal = (delai * 5) / 3;
    pthread_mutex_unlock(&mutexDelai);

    Attente(delaiLocal);

    pthread_mutex_lock(&mutexTab);
    // Tant qu'on n'a pas trouve une case ou qu'on n'a pas trop essaye (nombre choisi arbitrairement)
    while (!caseSuivanteTrouvee && tentative < 20)
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

      pthread_mutex_lock(&mutexMode);
      // Si on est dans le mode normal
      if (mode == 1)
      {
        pthread_mutex_unlock(&mutexMode);
        if (tab[nouveauL][nouveauC].presence != MUR && tab[nouveauL][nouveauC].presence != FANTOME)
        {
          // Restaurer l'ancienne case
          restaurerAncienneCase(l, c, cache);

          // Detecter s'il y a quelque chose sur la prochaine case
          detecterProchaineCaseFantome(nouveauL, nouveauC, cache);

          // Avancer le Fantome
          *l = nouveauL;
          *c = nouveauC;
          dir = nouvelleDir;

          setTab(*l, *c, FANTOME, pthread_self());
          DessineFantome(*l, *c, *couleur, dir);

          caseSuivanteTrouvee = true;
        }
        else
        {
          // Generer une direction aleatoire
          nouvelleDir = (rand() % 4) + 500000; // Note : Est-ce bien d'utiliser un nombre magique ?
          if (tentative < 10 && nouvelleDir == ((dir + 2) % 4) + 500000) // Donne un mouvement plus naturel aux Fantômes en leur evitant au maximum de faire demi-tour
          {
            nouvelleDir = (nouvelleDir + 1) % 4 + 500000;
          }
          tentative++;
        }
      }
      else // Si on est dans le mode Fantomes comestibles
      {
        pthread_mutex_unlock(&mutexMode);
        // S'il n'y a pas de mur, de fantome ou le PacMan dans cette direction
        if (tab[nouveauL][nouveauC].presence != MUR && tab[nouveauL][nouveauC].presence != FANTOME && tab[nouveauL][nouveauC].presence != PACMAN)
        {
          // Restaurer l'ancienne case
          restaurerAncienneCase(l, c, cache);

          // Detecter s'il y a quelque chose sur la prochaine case
          detecterProchaineCaseFantome(nouveauL, nouveauC, cache);

          // Avancer le Fantome
          *l = nouveauL;
          *c = nouveauC;
          dir = nouvelleDir;

          setTab(*l, *c, FANTOME, pthread_self());
          DessineFantomeComestible(*l, *c);

          caseSuivanteTrouvee = true;
        }
        else
        {
          // Generer une direction aleatoire
          nouvelleDir = (rand() % 4) + 500000; // Note : Est-ce bien d'utiliser un nombre magique ?
          if (tentative < 10 && nouvelleDir == ((dir + 2) % 4) + 500000) // Donne un mouvement plus naturel aux Fantômes en leur evitant au maximum de faire demi-tour
          {
            nouvelleDir = (nouvelleDir + 1) % 4 + 500000;
          }
          tentative++;
        }
      }
    }

    pthread_mutex_unlock(&mutexTab);
  }

  pthread_cleanup_pop(1);

  messageInfo("FANTOME", "Le thread se termine");
  pthread_exit(NULL);
}

void restaurerAncienneCase(int *l, int *c, int *cache)
{
  switch (*cache)
  {
    case VIDE:
      setTab(*l, *c, VIDE);
      EffaceCarre(*l, *c);
      break;

    case PACGOM:
      setTab(*l, *c, PACGOM);
      DessinePacGom(*l, *c);
      break;

    case SUPERPACGOM:
      setTab(*l, *c, SUPERPACGOM);
      DessineSuperPacGom(*l, *c);
      break;

    case BONUS:
      setTab(*l, *c, BONUS);
      DessineBonus(*l, *c);
      break;
  }
}

void detecterProchaineCaseFantome(int nouveauL, int nouveauC, int *cache)
{
  switch (tab[nouveauL][nouveauC].presence)
  {
    case VIDE:
      *cache = VIDE;
      break;

    case PACMAN:
      // Si le Pac-Man ne s'est pas tue au meme moment, tuer le PacMan
      if (tidPacMan)
      {
        EffaceCarre(nouveauL, nouveauC);
        setTab(nouveauL, nouveauC, VIDE);

        pthread_cancel(tidPacMan);
        messageInfo("FANTOME", "Le Fantome a tue le Pac-Man");
      }
      
      break;

    case PACGOM:
      *cache = PACGOM;
      break;

    case SUPERPACGOM:
      *cache = SUPERPACGOM;
      break;

    case BONUS:
      *cache = BONUS;
  }
}

void handlerSIGCHLD(int signal)
{
  pthread_exit(NULL);
}

void cleanupFantome(void *pParam)
{
  S_FANTOME *pStructFantome = (S_FANTOME *)pthread_getspecific(cle); 

  // Augmenter le score
  augmenterScore(50);

  // Augmenter le score s'il y a quelque chose cache
  switch (pStructFantome->cache)
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

  pthread_mutex_lock(&mutexNbFantomes);
  // Diminuer le nombre de Fantome correspondant
  switch (pStructFantome->couleur)
  {
    case ROUGE:
      nbRouge--;
      break;

    case VERT:
      nbVert--;
      break;

    case MAUVE:
      nbMauve--;
      break;

    case ORANGE:
      nbOrange--;
      break;
  }
  pthread_mutex_unlock(&mutexNbFantomes);
  pthread_cond_signal(&condNbFantomes);

  free(pStructFantome);
  messageSucces("FANTOME", "Memoire allouee pour structFantome liberee");

  return;
}

// *********************** Gestion du mode Fantomes comestibles ***********************

void* threadTimeOut(void *pParam)
{
  messageSucces("TIMEOUT", "Thread cree");

  // Demasquage des signaux SIGALRM et SIGQUIT
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGALRM);
  sigaddset(&mask, SIGQUIT);
  pthread_sigmask(SIG_UNBLOCK, &mask, NULL); // Note : On pourrait mettre SIG_SETMASK

  // Armement du signal SIGQUIT
  struct sigaction sa;
  sa.sa_handler = handlerSIGQUIT;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGQUIT, &sa, NULL) == -1)
  {
    messageErreur("TIMEOUT", "Erreur de sigaction\n");
    tidTimeOut = 0;
    pthread_exit(NULL);
  }

  sa.sa_handler = handlerSIGALRM;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGALRM, &sa, NULL) == -1)
  {
    messageErreur("TIMEOUT", "Erreur de sigaction\n");
    tidTimeOut = 0;
    pthread_exit(NULL);
  }

  int nbSecondesRestantes = *(int *)pParam;
  int nbSecondes = 8 + rand() % 8 + nbSecondesRestantes;
  printf("nbSecondes = %d\n", nbSecondes);
  free(pParam);

  alarm(nbSecondes);

  pthread_testcancel();

  pause();
  pthread_mutex_lock(&mutexMode);
  mode = 1;
  pthread_mutex_unlock(&mutexMode);
  pthread_cond_signal(&condMode);

  tidTimeOut = 0;
  printf("Mise a 0 de tidTimeOut du a un signal SIGALRM\n");
  messageInfo("TIMEOUT", "Le thread se termine");
  pthread_exit(NULL);
}

void handlerSIGQUIT(int signal)
{
  messageInfo("TIMEOUT", "Signal SIGQUIT recu");
  printf("Mise a 0 de tidTimeOut du a un signal SIGQUIT\n");
  pthread_exit(NULL);
}

void handlerSIGALRM(int signal)
{
  messageInfo("TIMEOUT", "Signal SIGALRM recu");
}

// *********************** Gestion des vies du Pac-Man ***********************

void* threadVies(void *pParam)
{
  messageSucces("VIES", "Thread cree");
  
  int nbVies = 3;

  // Tant que le Pac-Man a des vies
  while (nbVies > 0)
  {
    // Affiche le nombre de vies restantes
    DessineChiffre(18, 22, nbVies);
    // Cree le Pac-Man
    pthread_create(&tidPacMan, NULL, threadPacMan, NULL);
    // Attend la mort du Pac-Man
    pthread_join(tidPacMan, NULL); // Note : Attention ! Est-ce que Pac-Man peut mourir 2 fois en même temps ?
    tidPacMan = 0; // Empêche qu'un mauvais tid soit utilises
    
    pthread_mutex_lock(&mutexTab);
    // Efface la derniere case avec le Pac-Man
    if (tab[L][C].presence == PACMAN)
    {
      EffaceCarre(L, C);
      setTab(L, C, VIDE);
    }
    pthread_mutex_unlock(&mutexTab);

    nbVies--;
  }

  DessineChiffre(18, 22, nbVies);

  continuerJeu = false;

  // Mettre les tid des Fantomes à 0, car il ne peuvent pas le faire eux-meme
  for (int i = 0; i < 8; i++)
  {
    tidFantomes[i] = 0;
  }

  Attente(1000);
  
  DessineGameOver(9, 4);

  tidVies = 0;
  messageInfo("VIES", "threadVies termine");
  pthread_exit(NULL);
}

// *********************** Gestion des evenements ***********************

void* threadEvent(void *pParam)
{
  messageSucces("EVENT", "Thread cree");
  
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
          if (tidPacMan) pthread_kill(tidPacMan, SIGUSR1);
          
          break;

        case KEY_DOWN :
          printf("Fleche bas !\n");
          if (tidPacMan) pthread_kill(tidPacMan, SIGUSR2);
          
          break;

        case KEY_RIGHT :
          printf("Fleche droite !\n");
          if (tidPacMan) pthread_kill(tidPacMan, SIGHUP);
          
          break;

        case KEY_LEFT :
          printf("Fleche gauche !\n");
          if (tidPacMan) pthread_kill(tidPacMan, SIGINT);
          
          break;
      }
    }
  }

  pthread_exit(NULL);
}

// *********************** Fonctions d'annulation des threads ***********************

void annulerThreads()
{
  
}

void annulerAutreThreads()
{
  if (tidTimeOut)
  {
    pthread_kill(tidTimeOut, SIGQUIT);
    tidTimeOut = 0; // Non necessaire
    messageInfo("EVENT", "Le threadTimeOut etait present. Il a donc ete annule.");
  }
  else
  {
    messageInfo("EVENT", "threadTimeOut inexistant. Il ne sera donc pas annule.");
  }

  if (tidPacGom)
  {
    if (pthread_cancel(tidPacGom) == 0) messageSucces("EVENT", "threadPacGom, threadBonus, threadCompteurFantomes et threadVies ont ete annule");
    else messageErreur("EVENT", "threadPacGom n'a pas pu etre annule");
    pthread_join(tidPacGom, NULL);
  }
  else
  {
    messageInfo("EVENT", "threadPacGom inexistant. Il ne sera donc pas annule.");
  }

  if (tidBonus)
  {
    if (pthread_cancel(tidBonus) == 0) messageSucces("EVENT", "Le threadBonus a ete annule");
    else messageErreur("EVENT", "Le threadBonus n'a pas pu etre annule");
    pthread_join(tidBonus, NULL);
  }
  else
  {
    messageInfo("EVENT", "threadBonus inexistant. Il ne sera donc pas annule.");
  }

  if (tidVies)
  {
    if (pthread_cancel(tidVies) == 0) messageSucces("EVENT", "Le threadVies a ete annule");
    else messageErreur("EVENT", "Le threadVies n'a pas pu etre annule");
    pthread_join(tidVies, NULL);
  }
  else
  {
    messageInfo("EVENT", "threadVies inexistant. Il ne sera donc pas annule.");
  }

  // Si le threadPacMan existe
  if (tidPacMan)
  { 
    if (pthread_cancel(tidPacMan) == 0) messageSucces("EVENT", "Le threadPacMan a ete annule");
    else messageErreur("EVENT", "Le threadPacMan n'a pas pu etre annule");
    pthread_join(tidPacMan, NULL);
  }
  else
  {
    messageInfo("EVENT", "threadPacMan inexistant. Il ne sera donc pas annule.");
  }

  if (tidCompteurFantomes)
  {
    if (pthread_cancel(tidCompteurFantomes) == 0) messageSucces("EVENT", "Le threadCompteurFantomes a ete annule");
    else messageErreur("EVENT", "Le threadCompteurFantomes n'a pas pu etre annule");
    pthread_join(tidCompteurFantomes, NULL);
  }
  else
  {
    messageInfo("EVENT", "threadCompteurFantomes inexistant. Il ne sera donc pas annule.");
  }

  if (tidScore)
  {
    if (pthread_cancel(tidScore) == 0) messageSucces("EVENT", "threadScore a ete annule");
    else messageErreur("EVENT", "Tous les threads n'ont pas pu etre annule");
    pthread_join(tidScore, NULL);
  }
  else
  {
    messageInfo("EVENT", "threadScore inexistant. Il ne sera donc pas annule.");
  }
}

void annulerThreadsFantomes()
{
  for (int i = 0; i < 8; i++)
  {
    // Si le Fantome n'existe pas
    if (tidFantomes[i] == 0)
    {
      messageInfo("EVENT", "Thread Fantome inexistant. Il ne sera donc pas annule.");
      continue;
    }

    // Sinon, tuer le Fantome
    if (pthread_cancel(tidFantomes[i]) == 0)
    {
      pthread_join(tidFantomes[i], NULL);
      messageSucces("EVENT", "Un thread Fantome a ete annule");
    }
    else
    {
      messageErreur("EVENT", "Un thread Fantome n'a pas pu etre annule");
    }

    tidFantomes[i] = 0;
  }
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

void DessineGrilleBase()
{
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
