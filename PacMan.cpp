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


pthread_t tidPacMan, tidEvent; // Note : tidEvent peut etre dans le main
pthread_mutex_t mutexTab;
int dir, ancienneDir; // Direction du PacMan

void* threadPacMan(void *pParam);
void calculerCoord(int direction, int l, int c, int *nouveauL, int *nouveauC);
void updatePacManPosition(int *l, int *c, int nouveauL, int nouveauC, int direction, sigset_t *mask);
void handlerSignaux(int signal);
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
  DessineGrilleBase();

  // Exemple d'utilisation de GrilleSDL et Ressources --> code a supprimer
  // DessinePacMan(17, 7, GAUCHE);  // Attention !!! tab n'est pas modifie --> a vous de le faire !!!
  // DessineChiffre(14, 25, 9);
  // DessineFantome(5, 9, ROUGE, DROITE);
  // DessinePacGom(7, 4);
  // DessineSuperPacGom(9, 5);
  // DessineFantomeComestible(13, 15);
  // DessineBonus(5, 15);

  // Initialisation de mutexTab
  if (pthread_mutex_init(&mutexTab, NULL) != 0)
  {
    messageErreur("MAIN", "Erreur de phtread_mutex_init");
    fflush(stdout);
    exit(1);
  }
  messageSucces("MAIN", "Initialisation du mutex reussi");

  // Masquage des signaux SIGINT, SIGHUP, SIGUSR1 et SIGUSR2
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  sigprocmask(SIG_SETMASK, &mask, NULL);

  // Creation de threadPacman
  pthread_create(&tidPacMan, NULL, threadPacMan, NULL);
  pthread_create(&tidEvent, NULL, threadEvent, NULL);
  
  // Boucle principale
  // ok = 0;
  // while(!ok)
  // {
  //   event = ReadEvent();
  //   if (event.type == CROIX) ok = 1;
  //   if (event.type == CLAVIER)
  //   {
  //     switch(event.touche)
  //     {
  //       case 'q' :
  //         pthread_kill(tidPacMan, SIGKILL);
  //         ok = 1;
  //         break;

  //       case KEY_UP :
  //         printf("Fleche haut !\n");
  //         dir = HAUT;
  //         break;

  //       case KEY_DOWN :
  //         printf("Fleche bas !\n");
  //         dir = BAS;
  //         break;

  //       case KEY_RIGHT :
  //         printf("Fleche droite !\n");
  //         dir = DROITE;
  //         break;

  //       case KEY_LEFT :
  //         printf("Fleche gauche !\n");
  //         dir = GAUCHE;
  //         break;
  //     }
  //   }
  // }

  pthread_join(tidEvent, NULL);

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

void* threadPacMan(void *pParam)
{
  int l = 15, c = 8;
  int nouveauL, nouveauC, nouvelleDir;
  dir = GAUCHE;

  sigset_t mask;
  // Demasquage des signaux SIGINT, SIGHUP, SIGUSR1 et SIGUSR2
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGUSR1);
  sigaddset(&mask, SIGUSR2);
  sigprocmask(SIG_UNBLOCK, &mask, NULL);

  // Armement des signaux SIGINT, SIGHUP, SIGUSR1 et SIUSR2
  struct sigaction sa;
  sa.sa_handler = handlerSignaux;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGHUP, &sa, NULL) == -1 || sigaction(SIGUSR1, &sa, NULL) == -1 || sigaction(SIGUSR2, &sa, NULL) == -1)
  {
    messageErreur("THREAD", "Erreur de sigaction. Des signaux pourraient ne pas fonctionner.\n");
  }

  // Placer le PacMan au point de départ
  setTab(l, c, PACMAN);
  DessinePacMan(l, c, GAUCHE);

  // Boucled principale
  while (1)
  {
    // Attendre 300 millisecondes et puis la reception d'un signal
    sigprocmask(SIG_SETMASK, &mask, NULL);
    Attente(300);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    // Lire la nouvelle direction
    nouvelleDir = dir;

    // Calculer la nouvelle position selon la nouvelle direction
    calculerCoord(nouvelleDir, l, c, &nouveauL, &nouveauC);

    // Si le prochain emplacement n'est pas un mur
    if (tab[nouveauL][nouveauC].presence != MUR)
    {
      // On deplace le PacMan avec la nouvelle direction
      updatePacManPosition(&l, &c, nouveauL, nouveauC, nouvelleDir, &mask);
      ancienneDir = nouvelleDir;
    }
    else
    {
      // On tente de se deplacer avec l'ancienne direction
      calculerCoord(ancienneDir, l, c, &nouveauL, &nouveauC);

      // Si le prochain emplacement avec l'ancienne direction n'est pas un mur
      if (tab[nouveauL][nouveauC].presence != MUR)
      {
        updatePacManPosition(&l, &c, nouveauL, nouveauC, ancienneDir, &mask);
      }
    }
  }
}

void calculerCoord(int direction, int l, int c, int *nouveauL, int *nouveauC)
{
  *nouveauL = l;
  *nouveauC = c;

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

// Sert a mettre à jour la position du PacMan et son affichage
void updatePacManPosition(int *l, int *c, int nouveauL, int nouveauC, int direction, sigset_t *mask)
{
    sigprocmask(SIG_SETMASK, mask, NULL);
    setTab(*l, *c, VIDE);
    EffaceCarre(*l, *c);

    *l = nouveauL;
    *c = nouveauC;

    setTab(*l, *c, PACMAN);
    DessinePacMan(*l, *c, direction);
    sigprocmask(SIG_UNBLOCK, mask, NULL);
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

// *********************** Gestion des evenements ***********************

void* threadEvent(void *pParam)
{
  EVENT_GRILLE_SDL event;

  int quit = 0;

  while(!quit)
  {
    event = ReadEvent();

    if (event.type == CROIX) quit = 1;

    if (event.type == CLAVIER)
    {
      switch(event.touche)
      {
        case 'q' :
          pthread_kill(tidPacMan, SIGKILL);
          pthread_join(tidPacMan, NULL); // Note : emplacement non definitif

          quit = 1;
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
  pthread_mutex_lock(&mutexTab);
  tab[l][c].presence = presence;
  tab[l][c].tid = tid;
  pthread_mutex_unlock(&mutexTab);
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
  printf("(%s %p) (INFO) %s\n", nomThread, pthread_self(), message);
  fflush(stdout);
}

void messageSucces(const char *nomThread, const char *message)
{
  printf("\033[32m(%s %p) (SUCCES) %s\033[0m\n", nomThread, pthread_self(), message);
  fflush(stdout);
}

void messageErreur(const char* nomThread, const char *message)
{
  fprintf(stderr, "\033[31m(%s %p) (ERREUR) %s\033[0m\n", nomThread, pthread_self(), message);

  if (errno != 0)
  {
    perror("Message ");
    errno = 0;
  }
}
