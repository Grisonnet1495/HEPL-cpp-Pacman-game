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


pthread_t tidPacGom, tidPacMan, tidScore, tidEvent; // Note : tidEvent peut etre dans le main
pthread_mutex_t mutexTab, mutexDelai, mutexNbPacGom, mutexScore;
pthread_cond_t condNbPacGom, condScore;
bool MAJScore = true;
int dir, ancienneDir; // Direction du PacMan
int nbPacGom, delai = 300, score = 0;

void* threadPacGom(void *pParam);
void* threadPacMan(void *pParam);
void calculerCoord(int direction, int l, int c, int *nouveauL, int *nouveauC);
void detecterPresenceProchaineCase(int l, int c);
void augmenterScore(int augmentation);
void changerPositionPacman(int *l, int *c, int nouveauL, int nouveauC, int direction, sigset_t *mask);
void handlerSignaux(int signal);
void* threadScore(void *pParam);
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

  // Initialisation de mutexTab et de mutexDelai
  if (pthread_mutex_init(&mutexTab, NULL) != 0 || pthread_mutex_init(&mutexDelai, NULL) != 0 || pthread_mutex_init(&mutexNbPacGom, NULL) != 0 || pthread_mutex_init(&mutexScore, NULL) != 0)
  {
    messageErreur("MAIN", "Erreur de phtread_mutex_init");
    fflush(stdout);
    exit(1);
  }
  messageSucces("MAIN", "Initialisation de mutexTab, mutexDelai, mutexNbPacGom et mutexScore reussi");

  // Initialisation des variables de condition condNbPacGom et condScore
  pthread_cond_init(&condNbPacGom, NULL);
  pthread_cond_init(&condScore, NULL);

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
  pthread_create(&tidPacMan, NULL, threadPacMan, NULL);
  pthread_create(&tidScore, NULL, threadScore, NULL);
  pthread_create(&tidEvent, NULL, threadEvent, NULL); // On le met en dernier pour empecher les comportements inattendus
  
  pthread_join(tidPacMan, NULL);
  pthread_join(tidPacGom, NULL);
  pthread_join(tidScore, NULL);
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

void* threadPacGom(void *pParam)
{
  int l, c, niveauJeu = 1;

  // Tant qu'on n'a pas atteint le niveau 10
  while (1) // Note : A modifier avec niveauJeu < 10
  {
    // Note : Mettre a jour la position du Pac-Man

    // Afficher le niveau actuel
    DessineChiffre(14, 22, niveauJeu);

    nbPacGom = 0;
    
    pthread_mutex_lock(&mutexNbPacGom);
    // Remplir les cases vides avec des Pac-Goms
    for (l = 0; l < NB_LIGNE; l++)
    {
      for (c = 0; c < NB_COLONNE; c++)
      {
        if (tab[l][c].presence == 0)
        {
          setTab(l, c, PACGOM);
          DessinePacGom(l, c);
          nbPacGom++;
        }
      }
    }

    // Mettre une case vide à l'emplacement initial du Pac-Man et dans le nid de fantomes
    setTab(15, 8, VIDE);
    EffaceCarre(15, 8);

    setTab(8, 8, VIDE);
    EffaceCarre(8, 8);
    setTab(9, 8, VIDE);
    EffaceCarre(9, 8);
    nbPacGom -= 2;

    // Ajouter les super Pac-Goms
    setTab(2, 1, SUPERPACGOM);
    DessineSuperPacGom(2, 1);
    setTab(2, 15, SUPERPACGOM);
    DessineSuperPacGom(2, 15);
    setTab(15, 1, SUPERPACGOM);
    DessineSuperPacGom(15, 1);
    setTab(15, 15, SUPERPACGOM);
    DessineSuperPacGom(15, 15);
    pthread_mutex_unlock(&mutexNbPacGom);

    pthread_mutex_lock(&mutexNbPacGom);
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

    // Augmenter le niveau du jeu de 1
    niveauJeu++;

    // Augmenter la vitess du Pac-Man par 2
    pthread_mutex_lock(&mutexDelai);
    delai /= 2;
    pthread_mutex_unlock(&mutexDelai);
  }
  
  pthread_exit(NULL);
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

  // Placer le Pac-Man au point de départ
  setTab(l, c, PACMAN);
  DessinePacMan(l, c, GAUCHE);

  // Boucled principale
  while (1)
  {
    // Attendre 300 millisecondes et puis la reception d'un signal
    sigprocmask(SIG_SETMASK, &mask, NULL);
    pthread_mutex_lock(&mutexDelai);
    Attente(delai);
    pthread_mutex_unlock(&mutexDelai);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    // Lire la nouvelle direction
    nouvelleDir = dir;

    // Calculer la nouvelle position selon la nouvelle direction
    calculerCoord(nouvelleDir, l, c, &nouveauL, &nouveauC);

    // Si le prochain emplacement n'est pas un mur
    if (tab[nouveauL][nouveauC].presence != MUR)
    {
      detecterPresenceProchaineCase(nouveauL, nouveauC);

      // On deplace le Pac-Man avec la nouvelle direction
      changerPositionPacman(&l, &c, nouveauL, nouveauC, nouvelleDir, &mask);
      ancienneDir = nouvelleDir;
    }
    else
    {
      // On tente de se deplacer avec l'ancienne direction
      calculerCoord(ancienneDir, l, c, &nouveauL, &nouveauC);

      // Si le prochain emplacement avec l'ancienne direction n'est pas un mur
      if (tab[nouveauL][nouveauC].presence != MUR)
      {
        detecterPresenceProchaineCase(nouveauL, nouveauC);

        // On deplace le Pac-Man avec l'ancienne direction
        changerPositionPacman(&l, &c, nouveauL, nouveauC, ancienneDir, &mask);
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

void detecterPresenceProchaineCase(int l, int c)
{
  if (tab[l][c].presence == PACGOM)
  {
    augmenterScore(1);
  }
  else if (tab[l][c].presence == SUPERPACGOM)
  {
    augmenterScore(5);
  }
}

void augmenterScore(int augmentation)
{
  pthread_mutex_lock(&mutexNbPacGom);
  nbPacGom--;
  pthread_mutex_unlock(&mutexNbPacGom);
  pthread_cond_signal(&condNbPacGom);

  pthread_mutex_lock(&mutexScore); // Note : oblige ?
  score += augmentation;
  MAJScore = true; // Indiquer que le score a ete mis a jour
  pthread_mutex_unlock(&mutexScore);
  pthread_cond_signal(&condScore);
}

// Sert a mettre à jour la position du PacMan et son affichage
void changerPositionPacman(int *l, int *c, int nouveauL, int nouveauC, int direction, sigset_t *mask)
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

  // Annulation de threadPacMan, threadPacGom et threadScore
  if (pthread_cancel(tidPacMan) != 0 || pthread_cancel(tidPacGom) != 0 || pthread_cancel(tidScore))
  {
    messageErreur("EVENT", "Tous les threads n'ont pas pu etre annule");
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
