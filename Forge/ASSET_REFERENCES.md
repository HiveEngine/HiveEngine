# Asset References — Architecture

## Objectif

Les composants ECS referencent les assets avec un pattern Unity-like : le type du champ encode
le type d'asset. L'inspecteur genere automatiquement le bon widget (thumbnail + picker + clear)
sans annotation manuelle.

## Design

### Dans les composants

```cpp
struct MeshReference
{
    nectar::StrongHandle<TextureAsset> m_albedo;
    nectar::StrongHandle<TextureAsset> m_normal;
    nectar::StrongHandle<MeshAsset>    m_mesh;
    float m_scale{1.f};
};
```

`StrongHandle<T>` est le type de reference d'asset. Il encode :
- Que c'est une reference d'asset (pas une valeur inline)
- Le type d'asset (`TextureAsset`, `MeshAsset`, etc.)
- Le lifetime (ref-counting RAII)

### Serialisation

`StrongHandle<T>` serialise le GUID de l'asset (128-bit). Au chargement,
le GUID est resolu via `AssetServer::Load<T>()`.

### Inspector (Qt / Forge)

La reflection C++26 (P2996) detecte `StrongHandle<T>` comme un type d'asset reference.
L'inspecteur instancie automatiquement un widget `AssetField` :

```
[ thumbnail 48x48 ] nom_asset.ext    [loupe] [x]
```

- Thumbnail : preview de l'asset (image pour textures, icone pour meshes)
- Nom : resolu depuis le GUID via la database
- Loupe : ouvre un picker filtre sur le type `T`
- X : clear la reference
- Drop target : accepte un drag depuis le content browser

### Detection automatique

```cpp
template <typename T>
concept IsAssetHandle = requires {
    typename T::AssetType;
    { std::declval<T>().Id() } -> std::same_as<nectar::AssetId>;
};
```

Dans `BuildFieldWidget`, si `IsAssetHandle<FieldType>` est vrai, on instancie un `AssetField`
au lieu d'un spinbox/label. Le type `T::AssetType` determine le filtre du picker.

## Prerequis

1. Reflection C++26 (P2996) dans Clang — remplace les macros `QUEEN_REFLECT`
2. `StrongHandle<T>` et `WeakHandle<T>` existent deja dans Nectar
3. Widget `AssetField` reutilisable (actuellement le code est dans `MaterialInspector`,
   a extraire dans un widget generique)
4. `AssetDatabase` accessible depuis l'inspector pour resoudre GUID -> path -> nom

## Compatibilite

- `StrongHandle<T>` / `WeakHandle<T>` sont deja definis dans
  `Nectar/include/nectar/core/asset_handle.h`
- Le `AssetServer` gere deja `Load<T>`, `Get<T>`, `Release<T>` par type
- Le `AssetDatabase` resout deja les paths depuis les GUIDs
- Le `ThumbnailCache` dans le content browser peut etre reutilise pour les previews
