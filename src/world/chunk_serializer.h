// -----------------------------------------------------------------------------
// Fichier : chunk_serializer.h
// Rôle : Sérialisation/désérialisation des chunks sur disque.
// Format binaire avec compression RLE pour un stockage efficace.
// -----------------------------------------------------------------------------
#pragma once

#include "chunk.h"
#include <string>

class ChunkSerializer
{
  public:
	/// Sauvegarde un chunk sur disque dans le dossier spécifié.
	/// Fichier : <dir>/chunk_<cx>_<cz>.bin
	static void save(const Chunk &chunk, const std::string &worldDir);

	/// Charge un chunk depuis le disque. Retourne false si le fichier n'existe pas.
	static bool load(Chunk &chunk, const std::string &worldDir);

  private:
	/// Génère le chemin du fichier pour un chunk donné
	static std::string getFilePath(int cx, int cz, const std::string &dir);

	// Magic number pour identifier le format de fichier
	static constexpr uint32_t MAGIC = 0x464C4348; // "FLCH" (FarLands CHunk)
	static constexpr uint32_t VERSION = 1;
};
