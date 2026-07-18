export const ICON_THUMBNAIL_SIZE = 34;

export function drawIconThumbnail(documentRef, image, region) {
  const canvas = documentRef.createElement("canvas");
  canvas.width = ICON_THUMBNAIL_SIZE;
  canvas.height = ICON_THUMBNAIL_SIZE;
  const context = canvas.getContext("2d");
  if (!context) return null;
  context.drawImage(
    image,
    region.x,
    region.y,
    region.w,
    region.h,
    0,
    0,
    ICON_THUMBNAIL_SIZE,
    ICON_THUMBNAIL_SIZE,
  );
  return canvas;
}
