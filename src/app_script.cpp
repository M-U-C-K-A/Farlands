// -----------------------------------------------------------------------------
// Fichier : app_script.cpp
// Rôle : Scripts de capture pour l'outillage (captures d'écran de la
// documentation, vérifications visuelles). Activé par la variable
// d'environnement FARLANDS_SCRIPT=chemin. Une commande par ligne, '#' pour
// les commentaires. Les mondes ouverts par un script ne sont jamais
// sauvegardés.
//
//   wait <s>                 attendre
//   waitload [max s]         attendre la fin du streaming (chunks + LOD)
//   play | menu              fermer / ouvrir le menu principal
//   tp <x> <y|surface> <z> [yaw pitch]   téléporter (surface : sur le sol)
//   look <yaw> <pitch>       orienter le regard
//   fly <0|1>                mode vol (caméra immobile)
//   preset <realistic|golden|horror|night>
//   set <clé> <valeur>       sun_elev, sun_az, sun_rel, rd, lod, fov, leaves,
//                            fps, vsync
//   shot <fichier.png>       capture (chemin relatif au script)
//   size <w> <h>             taille de la fenêtre
//   locate <village|temple|ruins|igloo|shrine|lighthouse> [échelle de distance]
//   options <onglet>         ouvre les options (0 Shaders .. 4 World)
//   biome <amplified|cherry|peaks|desert|taiga|forest>
//   world <graine> [amplifié 0|1]   nouveau monde
//   setblock <x> <y> <z> <id>  modifie un bloc (comme le joueur)
//   hud <0|1>                interface en jeu (barre, réticule)
//   stats [nom]              journalise FPS, temps GPU, chunks
//   quit
// -----------------------------------------------------------------------------
#include "app.h"
#include "core/logger.h"

#include <cmath>
#include <fstream>
#include <sstream>

void Application::loadScript(const std::string &path)
{
	std::ifstream in(path);
	if(!in)
		{
			LOG_ERROR("Script not found: " << path);
			return;
		}
	std::string line;
	while(std::getline(in, line))
		{
			if(auto hash = line.find('#'); hash != std::string::npos)
				line.resize(hash);
			std::istringstream ss(line);
			std::vector<std::string> cmd;
			for(std::string w; ss >> w;)
				cmd.push_back(w);
			if(!cmd.empty())
				m_script.push_back(std::move(cmd));
		}
	auto slash = path.find_last_of('/');
	m_scriptDir = slash == std::string::npos ? "." : path.substr(0, slash);
	LOG_INFO("Script: " << m_script.size() << " commands from " << path);
}

void Application::updateScript(float time)
{
	if(m_scriptPos >= m_script.size())
		return;
	if(time < m_scriptWaitUntil)
		return;
	if(m_scriptWaitLoad)
		{
			bool idle = m_world->getPendingJobs() == 0 && m_lod->pendingJobs() == 0
						&& m_world->getMeshedCount() > 0;
			if(!idle && time < m_scriptLoadDeadline)
				return;
			m_scriptWaitLoad = false;
			m_scriptWaitUntil = time + 0.6f; // derniers envois au GPU
			return;
		}

	// Exécute les commandes jusqu'à la prochaine attente
	while(m_scriptPos < m_script.size())
		try
		{
			const auto &c = m_script[m_scriptPos++];
			auto f = [&](size_t i, float def = 0.0f) {
				return i < c.size() ? std::stof(c[i]) : def;
			};
			const std::string &op = c[0];
			if(op == "wait")
				{
					m_scriptWaitUntil = time + f(1);
					return;
				}
			if(op == "waitload")
				{
					m_scriptWaitLoad = true;
					m_scriptLoadDeadline = time + f(1, 30.0f);
					m_scriptWaitUntil = time + 0.5f;
					return;
				}
			if(op == "play")
				m_menu.setActive(false);
			else if(op == "menu")
				m_menu.setActive(true);
			else if(op == "tp" && c.size() >= 4)
				{
					float x = f(1), z = f(3);
					float y = c[2] == "surface"
								? static_cast<float>(m_world->getGenerator().getHeightAt(
									static_cast<int>(std::floor(x)), static_cast<int>(std::floor(z))))
								: f(2);
					m_player.setPosition(glm::vec3(x, y, z));
					if(c.size() >= 6)
						m_player.setLook(f(4), f(5));
					m_spawned = true;
				}
			else if(op == "look")
				m_player.setLook(f(1), f(2));
			else if(op == "fly")
				m_player.setFlying(f(1) != 0.0f);
			else if(op == "preset" && c.size() >= 2)
				{
					ShaderPreset p = c[1] == "golden"   ? ShaderPreset::GoldenHour
									 : c[1] == "horror" ? ShaderPreset::Horror
									 : c[1] == "night"  ? ShaderPreset::Night
														: ShaderPreset::Realistic;
					applyPreset(m_settings, p);
					onPresetChanged();
				}
			else if(op == "set" && c.size() >= 3)
				{
					const std::string &k = c[1];
					float v = f(2);
					if(k == "sun_elev")
						m_settings.sunElevation = v;
					else if(k == "sun_az")
						m_settings.sunAzimuth = v;
					else if(k == "rd")
						m_settings.renderDistance = static_cast<int>(v);
					else if(k == "lod")
						m_settings.lodDistance = static_cast<int>(v);
					else if(k == "fov")
						m_settings.fov = v;
					else if(k == "leaves")
						m_settings.fallingLeaves = v != 0.0f;
					else if(k == "fps")
						m_settings.showFps = v != 0.0f;
					else if(k == "vsync")
						m_settings.vsync = v != 0.0f;
					else if(k == "sun_rel")
						{
							// Azimut du soleil relatif au regard (180 : dans le dos)
							glm::vec3 fr = m_player.getFrontVector();
							float view = glm::degrees(std::atan2(fr.x, fr.z));
							m_settings.sunAzimuth = std::fmod(view + v + 720.0f, 360.0f);
						}
					else
						LOG_WARNING("Script: unknown setting " << k);
				}
			else if(op == "shot" && c.size() >= 2)
				{
					std::string path = c[1][0] == '/' ? c[1] : m_scriptDir + "/" + c[1];
					m_renderer.requestScreenshot(path);
					glm::vec3 p = m_player.getPosition();
					LOG_NOTE("Script shot " << path << " at " << p.x << ", " << p.y << ", " << p.z);
					m_scriptWaitUntil = time + 0.2f;
					return;
				}
			else if(op == "locate" && c.size() >= 2)
				{
					const std::string &n = c[1];
					StructureType t = n == "village"	? StructureType::Village
									  : n == "temple"	? StructureType::DesertTemple
									  : n == "ruins"	? StructureType::Ruins
									  : n == "igloo"	? StructureType::Igloo
									  : n == "shrine"	? StructureType::Shrine
														: StructureType::Lighthouse;
					teleportToStructure(t, c.size() >= 3 ? f(2) : 1.0f);
				}
			else if(op == "biome" && c.size() >= 2)
				{
					const std::string &n = c[1];
					BiomeType b = n == "amplified" ? BiomeType::Amplified
								  : n == "cherry"  ? BiomeType::CherryGrove
								  : n == "peaks"   ? BiomeType::SnowyPeaks
								  : n == "desert"  ? BiomeType::Desert
								  : n == "taiga"   ? BiomeType::Taiga
												   : BiomeType::Forest;
					teleportToBiome(b);
				}
			else if(op == "setblock" && c.size() >= 5)
				m_world->setBlockAt(static_cast<int>(f(1)), static_cast<int>(f(2)), static_cast<int>(f(3)),
									static_cast<BlockType>(static_cast<int>(f(4))));
			else if(op == "options" && c.size() >= 2)
				{
					m_menu.setActive(true);
					m_menu.openOptions(static_cast<int>(f(1)));
				}
			else if(op == "stats")
				{
					const auto &rs = m_renderer.getStats();
					LOG_NOTE("Script stats " << (c.size() > 1 ? c[1] : "") << ": " << m_debug.fps << " fps, avg "
											 << m_debug.frameAvgMs << " ms, 1% low " << m_debug.low1Fps
											 << " fps, GPU " << rs.gpuMs << " ms, chunks "
											 << m_world->getChunkCount() << "/" << m_world->getMeshedCount()
											 << ", jobs " << m_world->getPendingJobs() << ", tris "
											 << rs.triangles << ", draws " << rs.drawCalls);
				}
			else if(op == "hud" && c.size() >= 2)
				m_hideHud = f(1) == 0.0f;
			else if(op == "world" && c.size() >= 2)
				createWorld(static_cast<uint32_t>(std::stoul(c[1])), c.size() >= 3 && c[2] == "1");
			else if(op == "size" && c.size() >= 3)
				glfwSetWindowSize(m_window, static_cast<int>(f(1)), static_cast<int>(f(2)));
			else if(op == "quit")
				glfwSetWindowShouldClose(m_window, true);
			else
				LOG_WARNING("Script: unknown command " << op);
		}
		catch(const std::exception &e)
		{
			LOG_WARNING("Script: bad arguments on command " << m_scriptPos << " (" << e.what() << ")");
		}
}
