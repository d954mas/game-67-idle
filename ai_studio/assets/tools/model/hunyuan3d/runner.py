import argparse
from pathlib import Path

import torch
from PIL import Image

from hy3dgen.shapegen import Hunyuan3DDiTFlowMatchingPipeline


VIEW_NAMES = ("front", "back", "left", "right")


def multiview(args):
    views_directory = Path(args.views).resolve()
    images = {
        name: Image.open(views_directory / f"{name}.png").convert("RGBA")
        for name in VIEW_NAMES
        if (views_directory / f"{name}.png").is_file()
    }
    if "front" not in images or len(images) < 2:
        raise ValueError("views folder must contain front.png and at least one additional named view")
    pipeline = Hunyuan3DDiTFlowMatchingPipeline.from_pretrained(
        "tencent/Hunyuan3D-2mv",
        subfolder="hunyuan3d-dit-v2-mv",
        variant="fp16",
    )
    mesh = pipeline(
        image=images,
        num_inference_steps=args.steps,
        octree_resolution=args.octree_resolution,
        num_chunks=args.num_chunks,
        generator=torch.manual_seed(args.seed),
        output_type="trimesh",
    )[0]
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    mesh.export(output)


def texture(args):
    import trimesh
    from hy3dgen.texgen import Hunyuan3DPaintPipeline

    torch.set_default_device("cpu")
    pipeline = Hunyuan3DPaintPipeline.from_pretrained("tencent/Hunyuan3D-2")
    pipeline.enable_model_cpu_offload()
    pipeline.models["multiview_model"].pipeline.vae.use_slicing = True
    mesh = trimesh.load(Path(args.mesh).resolve(), force="mesh")
    image = Image.open(Path(args.image).resolve()).convert("RGBA")
    textured_mesh = pipeline(mesh, image=image)
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    textured_mesh.export(output)


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    parser_multiview = subparsers.add_parser("multiview")
    parser_multiview.add_argument("--views", required=True)
    parser_multiview.add_argument("--output", required=True)
    parser_multiview.add_argument("--seed", type=int, default=1234)
    parser_multiview.add_argument("--steps", type=int, default=30)
    parser_multiview.add_argument("--octree-resolution", type=int, default=256)
    parser_multiview.add_argument("--num-chunks", type=int, default=20_000)
    parser_texture = subparsers.add_parser("texture")
    parser_texture.add_argument("--mesh", required=True)
    parser_texture.add_argument("--image", required=True)
    parser_texture.add_argument("--output", required=True)
    args = parser.parse_args()
    if args.command == "multiview":
        multiview(args)
    elif args.command == "texture":
        texture(args)


if __name__ == "__main__":
    main()
