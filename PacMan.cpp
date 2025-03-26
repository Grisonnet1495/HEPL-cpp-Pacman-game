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

#define NIVEAUMAX 6

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
sigset_t maskPacMan; // Note : Variable globale pour simplifier
bool MAJScore = true;
bool continuerJeu = true;
int L, C, dir; // Position et direction du PacMan
int nbPacGom, delai = 300, score = 0;
int nbRouge = 0, nbVert = 0, nbMauve = 0, nbOrange = 0;
int mode = 1;

// Gestion des PacGoms
void* threadPacGom(void *pParam);
void initialiserPacGoms();
void initialiserSuperPacGoms();
void diminuerNbPacGom();
void afficherNbPacGoms();
void augmenterNiveau();
// Gestion du Pac-Man
void* threadPacMan(void *pParam);
void placerPacManEtAttente();
void calculerCoord(int direction, int *nouveauL, int *nouveauC);
void detecterProchaineCasePacMan(int l, int c);
void changerPositionPacMan(int nouveauL, int nouveauC, int direction);
void handlerSignauxPacMan(int signal);
// Gestion du score
void* threadScore(void *pParam);
void augmenterScore(int augmentation);
// Gestion du Bonus
void* threadBonus(void *pParam);
// Gestion des Fantomes
void* threadCompteurFantomes(void *pParam);
bool allouerStructFantome(S_FANTOME *structFantomes[8], int i, int couleur);
void cleanupStructFantomes(void *pStructFantomes);
// Gestion d'un Fantome
void* threadFantome(void *pParam);
void restaurerAncienneCase(int *l, int *c, int *cache);
void detecterProchaineCaseFantome(int nouveauL, int nouveauC, int *cache);
void handlerSIGCHLD(int signal);
void cleanupFantome(void *pParam);
// Gestion du mode Fantomes comestibles
void* threadTimeOut(void *pParam);
void handlerSIGQUIT(int signal);
void handlerSIGALRM(int signal);
// Gestion des vies du Pac-Man
void* threadVies(void *pParam);
// Gestion des evenements
void* threadEvent(void *pParam);
// Fonctions d'annulation des threads
void annulerThreadsPrincipaux();
void annulerThreadsFantomes();
// Fonction d'attente
void Attente(int milli);
// Fonction pour initialiser la table
void DessineGrilleBase();
// Fonction pour placer les composants dans la table
void setTab(int l, int c, int presence = VIDE, pthread_t tid = 0);
// Fonctions d'affichage
void messageInfo(const char *nomThread, const char *message);
void messageSucces(const char *nomThread, const char *message);
void messageErreur(const char* nomThread, const char *message);

// *********************** Initialisation du programme et gestion des threads principaux ***********************

int main(int argc,char* argv[])
{
  // struct sigaction sigAct; // Note : Pas utilise
  // char ok; // Note : Pas utilise
 
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
  sigprocmask(SIG_SETMASK, &mask, NULL);

  // Creation d'une cle pour des variables specifiques
  pthread_key_create(&cle, NULL);

  // Creation de threadPacGom, threadScore, threadPacman, threadEvent, threadBonus, threadCompteurFantomes et threadVies
  pthread_create(&tidPacGom, NULL, threadPacGom, NULL);
  pthread_detach(tidPacGom);
  pthread_create(&tidScore, NULL, threadScore, NULL);
  pthread_detach(tidScore);
  pthread_create(&tidBonus, NULL, threadBonus, NULL);
  pthread_detach(tidBonus);
  pthread_create(&tidCompteurFantomes, NULL, threadCompteurFantomes, NULL);
  pthread_detach(tidCompteurFantomes);
  pthread_create(&tidVies, NULL, threadVies, NULL);
  pthread_detach(tidVies);
  pthread_create(&tidEvent, NULL, threadEvent, NULL);
  
  // Attente de threadEvent
  pthread_join(tidEvent, NULL);

  messageInfo("MAIN", "Attente de 1500 millisecondes...");
  // Attente(1500);
  // -------------------------------------------------------------------------
  
  // Fermeture de la fenetre
  messageInfo("MAIN", "Fermeture de la fenetre graphique...");
  FermetureFenetreGraphique();
  messageSucces("MAIN", "Fenetre graphique fermee");

  // Annuler tous les threads
  annulerThreadsFantomes();
  annulerThreadsPrincipaux();

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

// *********************** Gestion des PacGoms ***********************

void* threadPacGom(void *pParam)
{
  messageSucces("PACGOM", "Thread cree");

  int niveauJeu = 1;

  // Tant qu'on n'a pas atteint le niveau maximal
  while (niveauJeu != NIVEAUMAX)
  {
    // Afficher le niveau actuel
    DessineChiffre(14, 22, niveauJeu);

    // Initialiser les Pac-Goms
    pthread_mutex_lock(&mutexNbPacGom);
    initialiserPacGoms();
    if (niveauJeu == 1) initialiserSuperPacGoms(); // On ne genere les SuperPacGoms que pour le niveau 1

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

    niveauJeu++;

    // Si on a atteint le niveau maximal
    if (niveauJeu != NIVEAUMAX)
    {
      pthread_mutex_lock(&mutexTab);
      augmenterNiveau();
      pthread_mutex_unlock(&mutexTab);
    }
  }

  // Le joueur a gagne
  continuerJeu = false;

  Attente(1000);

  DessineVictory(9, 4);
  
  tidPacGom = 0;
  messageInfo("PACGOM", "Le thread se termine");
  pthread_exit(NULL);
}

void initialiserPacGoms()
{
  int l, c;
  int tabPosVide[3][2] = {{15, 8}, {8, 8}, {9, 8}}; // Cases devant etre vides

  pthread_mutex_lock(&mutexTab);
  nbPacGom = 0;
  
  // Remplir les cases vides avec des Pac-Goms
  for (l = 0; l < /*NB_LIGNE*/4; l++)
  {
    for (c = 0; c < /*NB_COLONNE*/4; c++)
    {
      if (tab[l][c].presence == VIDE)
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
  pthread_mutex_unlock(&mutexTab);
}

void initialiserSuperPacGoms()
{
  int l, c;
  int tabPosSuperPacGoms[4][2] = {{2, 1}, {2, 15}, {15, 1}, {15, 15}}; // Emplacement des Super Pac-Goms

  pthread_mutex_lock(&mutexTab);
  // Ajouter les super Pac-Goms
  for (unsigned long int i = 0; i < (sizeof(tabPosSuperPacGoms) / sizeof(tabPosSuperPacGoms[0])); i++)
  {
    l = tabPosSuperPacGoms[i][0];
    c = tabPosSuperPacGoms[i][1];

    if (tab[l][c].presence == PACGOM) // Si on n'a pas rempli toute les cases vides avec des Pac-Goms
    {
      nbPacGom--;
    }

    setTab(l, c, SUPERPACGOM);
    DessineSuperPacGom(l,c);
  }
  pthread_mutex_unlock(&mutexTab);
}

void diminuerNbPacGom()
{
  pthread_mutex_lock(&mutexNbPacGom);
  nbPacGom--;
  pthread_mutex_unlock(&mutexNbPacGom);
  pthread_cond_signal(&condNbPacGom);
}

void afficherNbPacGoms()
{
  DessineChiffre(12, 22, nbPacGom / 100);
  DessineChiffre(12, 23, (nbPacGom % 100) / 10);
  DessineChiffre(12, 24, nbPacGom % 10);
}

void augmenterNiveau()
{
  // Augmenter la vitess du Pac-Man par 2
  pthread_mutex_lock(&mutexDelai);
  delai = delai * 2 / 3; // Note : J'ai diminue l'augmentation du delai pour plus de confort
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
  if (tidTimeOut)
  {
    pthread_kill(tidTimeOut, SIGQUIT);
    tidTimeOut = 0;

    alarm(0);

    pthread_mutex_lock(&mutexMode);
    mode = 1;
    pthread_mutex_unlock(&mutexMode);
    pthread_cond_signal(&condMode);
  }

  // Efface l'emplacement courant du Pac-Man
  EffaceCarre(L, C);
  setTab(L, C, VIDE);
  // Remet le Pac-Man a la position de depart
  placerPacManEtAttente();

  changerPositionPacMan(LENTREE, CENTREE, GAUCHE);
}

// *********************** Gestion du Pac-Man ***********************

void* threadPacMan(void *pParam)
{
  messageSucces("PACMAN", "Thread cree");
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL); // Empeche que le thread ne puisse pas etre annule s'il est bloque sur un mutex.

  int nouveauL, nouveauC, nouvelleDir, ancienneDir = GAUCHE;
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
  sa.sa_handler = handlerSignauxPacMan;
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
    // Recuperer la valeur de la variable delai
    pthread_mutex_lock(&mutexDelai);
    delaiLocal = delai; // Note : On la recupere pour ne pas bloquer tout le monde pendant qu'on attend
    pthread_mutex_unlock(&mutexDelai);

    // Attend le temps du delai
    sigprocmask(SIG_BLOCK, &maskPacMan, NULL); // Pour pas que le PacMan ne puisse etre derange
    Attente(delaiLocal);
    sigprocmask(SIG_UNBLOCK, &maskPacMan, NULL);

    /***********************************************
     * Note :
     * L'algorithme de deplacement du PacMan a ete
     * concu specialement pour que l'on puisse
     * appuyer en avance sur la touche et que, des
     * que il est possible de se deplacer dans la
     * direction demandee, le PacMan le fasse.
     * Il implemente aussi les tunnels a gauche et
     * a droite du niveau.
     ***********************************************/

    // Lire la nouvelle direction
    nouvelleDir = dir;

    pthread_mutex_lock(&mutexTab);
    // Calculer la nouvelle position selon la nouvelle direction
    calculerCoord(nouvelleDir, &nouveauL, &nouveauC);

    // Si on est au bout du tunnel de gauche
    if (L == 9 && C == 0 && nouvelleDir == GAUCHE)
    {
      changerPositionPacMan(9, 16, nouvelleDir);
    }
    else if (L == 9 && C == 16 && nouvelleDir == DROITE) // Si on est au bout du tunnel de droite
    {
      changerPositionPacMan(9, 0, nouvelleDir);
    }
    else if (tab[nouveauL][nouveauC].presence != MUR) // Si le prochain emplacement n'est pas un mur
    {
      detecterProchaineCasePacMan(nouveauL, nouveauC);

      changerPositionPacMan(nouveauL, nouveauC, nouvelleDir);
      ancienneDir = nouvelleDir;
    }
    else // Si la nouvelle direction n'est pas possible
    {
      // On tente de se deplacer avec l'ancienne direction
      calculerCoord(ancienneDir, &nouveauL, &nouveauC);

      // Si le prochain emplacement avec l'ancienne direction n'est pas un mur
      if (tab[nouveauL][nouveauC].presence != MUR)
      {
        detecterProchaineCasePacMan(nouveauL, nouveauC);

        changerPositionPacMan(nouveauL, nouveauC, ancienneDir);
      }
    }
    pthread_mutex_unlock(&mutexTab);
  }

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
      // diminuerNbPacGom(); // Note : On ne le fait pas expres (pour augmenter la difficulte)
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

        pthread_t tidFantome = tab[l][c].tid;
        // Tuer le Fantome
        pthread_kill(tidFantome, SIGCHLD);
      }
      
      break;
  }
}

// Sert a mettre à jour la position du PacMan et son affichage
void changerPositionPacMan(int nouveauL, int nouveauC, int direction)
{
  sigprocmask(SIG_BLOCK, &maskPacMan, NULL);
  setTab(L, C, VIDE);
  EffaceCarre(L, C);

  L = nouveauL;
  C = nouveauC;

  setTab(L, C, PACMAN);
  DessinePacMan(L, C, direction);
  sigprocmask(SIG_UNBLOCK, &maskPacMan, NULL);
}

// Change la direction du Pac-Man a la reception d'un signal
void handlerSignauxPacMan(int signal)
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
    pthread_mutex_lock(&mutexScore);
    // Tant que la score n'a pas ete mis a jour
    while (MAJScore == false)
    {
      pthread_cond_wait(&condScore, &mutexScore); // Attendre un changement du score
    }
    pthread_mutex_unlock(&mutexScore);

    // Afficher le score
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

void augmenterScore(int augmentation)
{
  pthread_mutex_lock(&mutexScore);
  score += augmentation;
  MAJScore = true;
  pthread_mutex_unlock(&mutexScore);
  pthread_cond_signal(&condScore);
}

// *********************** Gestion du Bonus ***********************

void* threadBonus(void *pParam)
{
  messageSucces("BONUS", "Thread cree");
  
  int l, c, lDepart, cDepart;
  bool bonusPlace;  // Flag pour verifier si une case vide a ete trouvee

  while (continuerJeu)
  {
    // Attendre un duree aleatoire
    Attente((rand() % 11 + 10) * 1000);

    // Generer un emplacement de depart aleatoire
    lDepart = rand() % NB_LIGNE;
    cDepart = rand() % NB_COLONNE;

    bonusPlace = false;

    pthread_mutex_lock(&mutexTab);
    // Si le jeu s'est termine pendant l'attente
    if (!continuerJeu)
    {
      pthread_mutex_unlock(&mutexTab);
      break;
    }

    /***********************************************
     * Note :
     * J'ai fait cet algorithme car il est un peu
     * plus optimise qu'une generation aleatoire
     * classique (pour le challenge, donc).
     * Cependant, cela aurait tout aussi bien
     * fonctionne dans ce cas.
     ***********************************************/

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
    Attente(10000);

    // Vu que la boucle a incremente les valeurs de l et c de 1 a la sortie, on les decremente de 1
    l--;
    c--;

    pthread_mutex_lock(&mutexTab);
    // Si le jeu s'est termine pendant l'attente
    if (!continuerJeu)
    {
      pthread_mutex_unlock(&mutexTab);
      break;
    }

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
  int i;

  int delaiLocal;
  bool creerFantomePossible = false;

  // Boucle principale
  while (continuerJeu)
  {
    pthread_mutex_lock(&mutexDelai);
    delaiLocal = (delai * 5) / 3; // On recupere delai pour ne pas bloquer tout le monde pendant l'attente
    pthread_mutex_unlock(&mutexDelai);

    Attente(delaiLocal);

    pthread_mutex_lock(&mutexNbFantomes);
    // Tant qu'il y a tous les Fantomes
    while (nbRouge == 2 && nbVert == 2 && nbMauve == 2 && nbOrange == 2)
    {
      pthread_cond_wait(&condNbFantomes, &mutexNbFantomes);
    }
    pthread_mutex_unlock(&mutexNbFantomes);

    pthread_mutex_lock(&mutexMode);
    // Tant qu'on n'est dans le mode Fantomes comestibles
    while (mode == 2)
    {
      pthread_cond_wait(&condMode, &mutexMode);
    }
    pthread_mutex_unlock(&mutexMode);

    if (!continuerJeu)
    {
      break;
    }

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
      if (pthread_create(&tidFantomes[i], NULL, threadFantome, (void*)structFantomes[i]) == 0)
      {
        pthread_detach(tidFantomes[i]);
      }
      else
      {
        messageErreur("COMPTEURFANTOMES", "Erreur de pthread_create");
      }
    }
    else
    {
      messageInfo("COMPTEURFANTOMES", "Impossible de creer le threadFantome");
    }
  }

  tidCompteurFantomes = 0;
  messageInfo("COMPTEURFANTOMES", "Thread termine");
  pthread_exit(NULL);
}

bool allouerStructFantome(S_FANTOME *structFantomes[8], int i, int couleur)
{
  if (tidFantomes[i]) return false;

  structFantomes[i] = (S_FANTOME*)malloc(sizeof(S_FANTOME));

  // Si la memoire n'a pas ete alloue
  if (structFantomes[i] == NULL)
  {
    return false;
  }

  // Initialiser le Fantome
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
  int dir = HAUT; // Note : Se substitue la variable dir globale
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
  pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

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
    caseSuivanteTrouvee = false;
    tentative = 0;

    // Attente d'un delai
    pthread_mutex_lock(&mutexDelai);
    delaiLocal = (delai * 5) / 3;
    pthread_mutex_unlock(&mutexDelai);

    Attente(delaiLocal);

    /***********************************************
     * Note :
     * Cet algorithme de deplacement est
     * specialement concue pour donner un air plus
     * naturel au deplacement des Fantomes
     ***********************************************/

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
          nouvelleDir = (rand() % 4) + 500000; // Note : On rajoute 500000 pour transformer le nombre et direction
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
          nouvelleDir = (rand() % 4) + 500000; // Note : On rajoute 500000 pour transformer le nombre et direction
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
      // Si le Pac-Man ne s'est pas tue, tuer le PacMan
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

  // Liberer la memoire pour la structFantome du Fantome
  free(pStructFantome);
  messageSucces("FANTOME", "Memoire allouee pour structFantome liberee");

  // Mettre le tid du Fantome a 0
  for (int i = 0; i < 8; i++)
  {
    if (tidFantomes[i] == pthread_self())
    {
      tidFantomes[i] = 0;
    }
  }

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
  pthread_sigmask(SIG_UNBLOCK, &mask, NULL);

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

  // Armement du signal SIGALRM
  sa.sa_handler = handlerSIGALRM;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGALRM, &sa, NULL) == -1)
  {
    messageErreur("TIMEOUT", "Erreur de sigaction\n");
    tidTimeOut = 0;
    pthread_exit(NULL);
  }

  // Calculer le nombre de secondes de l'alarme
  int nbSecondesRestantes = *(int *)pParam;
  int nbSecondes = 8 + rand() % 8 + nbSecondesRestantes;
  free(pParam);

  // Lancer l'alarme
  alarm(nbSecondes);

  // Attendre la reception d'un signal
  pause();

  pthread_mutex_lock(&mutexMode);
  mode = 1;
  pthread_mutex_unlock(&mutexMode);
  pthread_cond_signal(&condMode);

  tidTimeOut = 0;
  messageInfo("TIMEOUT", "Le thread se termine");
  pthread_exit(NULL);
}

void handlerSIGQUIT(int signal)
{
  messageInfo("TIMEOUT", "Signal SIGQUIT recu");
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
    pthread_join(tidPacMan, NULL);
    tidPacMan = 0;

    // Si le jeu s'arrete
    if (!continuerJeu)
    {
      tidVies = 0;
      messageInfo("VIES", "threadVies termine");
      pthread_exit(NULL);
    }
    
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

  // Le joueur a perdu
  DessineChiffre(18, 22, nbVies);

  continuerJeu = false;

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
          continuerJeu = false;
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

void annulerThreadsPrincipaux()
{
  // Annuler les threads principaux s'ils existent
  if (tidTimeOut)
  {
    pthread_kill(tidTimeOut, SIGQUIT);
    tidTimeOut = 0; // Note : Non necessaire
    messageInfo("EVENT", "Le threadTimeOut etait present. Il a donc ete annule.");
  }
  else
  {
    messageInfo("EVENT", "Le threadTimeOut est deja termine");
  }

  if (tidPacGom)
  {
    if (pthread_cancel(tidPacGom) == 0) messageSucces("EVENT", "Le threadPacGom a ete annule");
    else messageErreur("EVENT", "Le threadPacGom n'a pas pu etre annule");
  }
  else
  {
    messageInfo("EVENT", "Le threadPacGom est deja termine");
  }

  if (tidBonus)
  {
    if (pthread_cancel(tidBonus) == 0) messageSucces("EVENT", "Le threadBonus a ete annule");
    else messageErreur("EVENT", "Le threadBonus n'a pas pu etre annule");
  }
  else
  {
    messageInfo("EVENT", "Le threadBonus est deja termine");
  }

  if (tidVies)
  {
    if (pthread_cancel(tidVies) == 0) messageSucces("EVENT", "Le threadVies a ete annule");
    else messageErreur("EVENT", "Le threadVies n'a pas pu etre annule");
  }
  else
  {
    messageInfo("EVENT", "Le threadVies est deja termine");
  }

  if (tidPacMan)
  { 
    if (pthread_cancel(tidPacMan) == 0) messageSucces("EVENT", "Le threadPacMan a ete annule");
    else messageErreur("EVENT", "Le threadPacMan n'a pas pu etre annule");
    pthread_join(tidPacMan, NULL);
  }
  else
  {
    messageInfo("EVENT", "Le threadPacMan est deja termine");
  }

  if (tidCompteurFantomes)
  {
    if (pthread_cancel(tidCompteurFantomes) == 0) messageSucces("EVENT", "Le threadCompteurFantomes a ete annule");
    else messageErreur("EVENT", "Le threadCompteurFantomes n'a pas pu etre annule");
  }
  else
  {
    messageInfo("EVENT", "Le threadCompteurFantomes est deja termine");
  }

  if (tidScore)
  {
    if (pthread_cancel(tidScore) == 0) messageSucces("EVENT", "Le threadScore a ete annule");
    else messageErreur("EVENT", "Le threadScore n'a pas pu etre annule");
  }
  else
  {
    messageInfo("EVENT", "Le threadScore est deja termine");
  }
}

void annulerThreadsFantomes()
{
  for (int i = 0; i < 8; i++)
  {
    // Si le Fantome existe
    if (tidFantomes[i])
    {
      if (pthread_cancel(tidFantomes[i]) == 0)
      {
        messageSucces("EVENT", "Un thread Fantome a ete annule");
      }
      else
      {
        messageInfo("EVENT", "Un thread Fantome etait deja annule");
      }

      tidFantomes[i] = 0;
    }
    else
    {
      messageInfo("EVENT", "Thread Fantome inexistant. Il ne sera donc pas annule.");
    }
  }
}

// *********************** Fonction d'attente ***********************

void Attente(int milli)
{
  struct timespec del;
  del.tv_sec = milli / 1000;
  del.tv_nsec = (milli % 1000) * 1000000;
  nanosleep(&del, NULL);
}

// *********************** Fonction pour placer les composants dans la table ***********************

void setTab(int l, int c, int presence, pthread_t tid)
{
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
