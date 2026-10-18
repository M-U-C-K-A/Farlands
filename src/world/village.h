// -----------------------------------------------------------------------------
// Fichier : village.h
// Rôle : Plan des grands villages : place pavée avec fontaine et étals,
// rues principales et ruelles, maisons de plusieurs tailles, église avec
// clocher, forge, taverne, bibliothèque, grange, moulin, champs le long des
// chemins, remparts avec tours et portes pour les plus grands, port avec
// pontons et barques au bord de l'eau.
// -----------------------------------------------------------------------------
#pragma once

#include "structures.h"

class TerrainGenerator;

namespace village
	{
		/// Construit le plan complet du village du site (déterministe)
		void build(const TerrainGenerator &gen, const StructureSite &site, StructureBlueprint &bp);
	}
