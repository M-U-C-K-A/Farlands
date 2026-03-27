#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Définition de la taille de la grille de gradients
#define GRID_SIZE 256

// Tableau global pour stocker les vecteurs de gradient (x, y)
float gradients[GRID_SIZE][GRID_SIZE][2];

/*
 * @brief 1. Initialise les gradients de la grille.
 * @brief Remplit la grille avec des vecteurs aléatoires de longueur 1.
 * @brief C'est ce qui donne l'aspect "organique" au bruit.
 */
void initGradients(void)
{
	for (int i = 0; i < GRID_SIZE; i++) {
		for (int j = 0; j < GRID_SIZE; j++) {
			float angle = (float)rand() / (float)RAND_MAX * 2.0 * M_PI;
			gradients[i][j][0] = cos(angle);
			gradients[i][j][1] = sin(angle);
		}
	}
}

/*
 * @brief 2. Lissage (smoothstep).
 * @brief Utilise l'équation $6t^5 - 15t^4 + 10t^3$ ou $3t^2 - 2t^3$.
 * @brief Permet d'éviter des transitions trop brusques entre les cases de la
 * grille.
 */
float smoothstep(float t)
{
	return t * t * (3.0 - 2.0 * t);
}

/*
 * @brief 3. Interpolation.
 * @brief Mélange deux valeurs a0 et a1 selon un poids w (entre 0 et 1).
 */
float interpolate(float a0, float a1, float w)
{
	return a0 + (a1 - a0) * smoothstep(w);
}

/*
 * @brief 4. Produit scalaire (dot product).
 * @brief Calcule l'impact d'un coin de la grille (ix, iy) sur le point (x,y).
 * @brief On projette le vecteur de distance sur le vecteur de gradient.
 */
float dotGridGradient(int ix, int iy, float x, float y)
{
	float dx = x - (float)ix;
	float dy = y - (float)iy;

	// Utilisation du modulo pour boucler sur le tableau si les coordonnées
	// dépassent
	int tx = ix % (GRID_SIZE - 1);
	int ty = iy % (GRID_SIZE - 1);

	return (dx * gradients[ty][tx][0] + dy * gradients[ty][tx][1]);
}

/*
 * @brief 5. Calcul du bruit de Perlin.
 * @brief La fonction principale qui coordonne l'interpolation entre les 4
 * coins d'une case.
 */
float perlin(float x, float y)
{
	int x0 = (int)floor(x);
	int x1 = x0 + 1;
	int y0 = (int)floor(y);
	int y1 = y0 + 1;

	float sx = x - (float)x0;
	float sy = y - (float)y0;

	float n0, n1, ix0, ix1, value;

	n0 = dotGridGradient(x0, y0, x, y);
	n1 = dotGridGradient(x1, y0, x, y);
	ix0 = interpolate(n0, n1, sx);

	n0 = dotGridGradient(x0, y1, x, y);
	n1 = dotGridGradient(x1, y1, x, y);
	ix1 = interpolate(n0, n1, sx);

	value = interpolate(ix0, ix1, sy);

	// Le bruit de Perlin est généralement entre -1.0 et 1.0.
	// On le ramène entre 0.0 et 1.0 pour plus de facilité.
	return (value + 1.0) / 2.0;
}

/*
 * @brief 6. Programme principal.
 * @brief Initialise l'aléatoire et génère la table de gradients.
 */
int main(int argc, char **argv)
{
	srand(time(NULL));	// Initialise l'aléatoire
	initGradients();	// Génère la table de gradients

	if (argc != 3) {
		printf("Usage: %s x y (ex: %s 12.5 45.8)\n", argv[0], argv[0]);
		return 1;
	}

	float x = atof(argv[1]);
	float y = atof(argv[2]);

	// Pour obtenir 3 bruits différents, on décale simplement les coordonnées.
	// C'est l'équivalent de 3 zones différentes d'une immense carte infinie.
	float hauteur = perlin(x, y);
	float temp = perlin(x + 500.5, y + 500.5);		  // Offset
	float humidite = perlin(x + 1000.1, y + 1000.1);  // Offset

	printf("--- Resultats pour (%.2f, %.2f) ---\n", x, y);
	printf("Hauteur    : %.2f\n", hauteur);
	printf("Temperature: %.2f\n", temp);
	printf("Humidite   : %.2f\n", humidite);

	return 0;
}
