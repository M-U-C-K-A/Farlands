import os
from PIL import Image

def slice_atlas():
    base_dir = "/Users/admin/Desktop/minecraft_2.0/assets"
    atlas_path = os.path.join(base_dir, "textures/atlas.png")
    out_dir = os.path.join(base_dir, "textures/block")

    if not os.path.exists(out_dir):
        os.makedirs(out_dir)

    # Dictionary representing the hardcoded logic that was in chunk_mesh.cpp
    # Each ID had: side at (id*16, 0), top at (id*16, 16)
    # The actual atlas image is loaded.
    try:
        atlas = Image.open(atlas_path).convert("RGBA")
    except Exception as e:
        print(f"Failed to open atlas: {e}")
        return

    # Assuming 16x16 tile sizes
    TILE_SIZE = 16

    blocks = {
        1: ("dirt", True),
        2: ("grass_block", True),  # Grass has side and top, bottom is dirt
        3: ("stone", False),
        4: ("oak_log", True),
        5: ("oak_planks", False),
        6: ("glass", False),
        7: ("oak_leaves", False),
        8: ("sand", False),
        9: ("water_still", False),
        10: ("cobblestone", False),
        11: ("bedrock", False),
        12: ("gravel", False),
        13: ("iron_ore", False),
        14: ("coal_ore", False),
    }

    for block_id, (name, has_top) in blocks.items():
        # Crop Side
        side_box = (block_id * TILE_SIZE, 0, (block_id + 1) * TILE_SIZE, TILE_SIZE)
        side_img = atlas.crop(side_box)
        
        # If it doesn't have a top, we assume it's uniform
        if has_top:
            side_img.save(os.path.join(out_dir, f"{name}_side.png"))
            top_box = (block_id * TILE_SIZE, TILE_SIZE, (block_id + 1) * TILE_SIZE, 2 * TILE_SIZE)
            top_img = atlas.crop(top_box)
            top_img.save(os.path.join(out_dir, f"{name}_top.png"))
        else:
            side_img.save(os.path.join(out_dir, f"{name}.png"))

    # Missing variants / mappings
    # For Grass bottom, we use dirt.png which is already created above.
    
    # Save a generic "air" texture ? Not needed, air is not rendered.
    print(f"Successfully sliced atlas into {out_dir}")

if __name__ == "__main__":
    slice_atlas()
