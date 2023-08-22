use std::sync::Arc;

use cubing::kpuzzle::{InvalidDefinitionError, KPuzzle, KPuzzleOrbitDefinition, KPuzzleOrbitName};

use super::PackedKState;

#[derive(Debug)]
pub struct PackedKPuzzleOrbitData {
    pub name: KPuzzleOrbitName,
    pub definition: KPuzzleOrbitDefinition,
    pub bytes_offset: usize,
}

impl Clone for PackedKPuzzleOrbitData {
    fn clone(&self) -> Self {
        Self {
            name: self.name.clone(),
            // TODO: Implement `Clone` for `KPuzzleOrbitDefinition`?
            definition: KPuzzleOrbitDefinition {
                num_pieces: self.definition.num_pieces,
                num_orientations: self.definition.num_orientations,
            },
            bytes_offset: self.bytes_offset,
        }
    }
}

#[derive(Debug, Clone)]
pub struct PackedKPuzzleData {
    pub kpuzzle: KPuzzle,
    // Private cached values.
    pub num_bytes: usize,
    pub orbit_iteration_data: Vec<PackedKPuzzleOrbitData>,
}

#[derive(Debug, Clone)]
pub struct PackedKPuzzle {
    pub data: Arc<PackedKPuzzleData>,
}

impl TryFrom<KPuzzle> for PackedKPuzzle {
    type Error = InvalidDefinitionError;

    fn try_from(kpuzzle: KPuzzle) -> Result<Self, Self::Error> {
        let def = kpuzzle.definition();
        let orbit_ordering = &def.orbit_ordering;
        let orbit_ordering = orbit_ordering.as_ref().ok_or_else(|| InvalidDefinitionError{ description: "Constructing a `PackedKPuzzle` from a `KPuzzle` requires the `orbitOrdering` field.".to_owned()})?;

        let mut bytes_offset = 0;
        let mut orbit_iteration_data: Vec<PackedKPuzzleOrbitData> = vec![];

        for orbit_name in orbit_ordering {
            let orbit_definition = kpuzzle.definition().orbits.get(orbit_name);
            let orbit_definition = orbit_definition.ok_or_else(|| InvalidDefinitionError {
                description: format!(
                    "Missing orbit definition for orbit in ordering: {}",
                    orbit_name
                ),
            })?;
            orbit_iteration_data.push({
                PackedKPuzzleOrbitData {
                    name: orbit_name.clone(),
                    definition: KPuzzleOrbitDefinition {
                        num_pieces: orbit_definition.num_pieces,
                        num_orientations: orbit_definition.num_orientations,
                    },
                    bytes_offset,
                }
            });
            bytes_offset += orbit_definition.num_pieces * 2;
        }

        Ok(Self {
            data: Arc::new(PackedKPuzzleData {
                kpuzzle,
                num_bytes: bytes_offset,
                orbit_iteration_data,
            }),
        })
    }
}

impl PackedKPuzzle {
    pub fn start_state(&self) -> PackedKState {
        let kstate_start_state_data = self.data.kpuzzle.start_state().state_data;
        let mut bytes: Vec<u8> = vec![0; self.data.num_bytes];

        for current_orbit in &self.data.orbit_iteration_data {
            let kstate_orbit_data = kstate_start_state_data
                .get(&current_orbit.name)
                .expect("Missing orbit!");
            let num_pieces = current_orbit.definition.num_pieces;
            for i in 0..num_pieces {
                bytes[current_orbit.bytes_offset + i] = kstate_orbit_data.pieces[i]
                    .try_into()
                    .expect("Value too large!");
                // TODO: macro?
                bytes[current_orbit.bytes_offset + num_pieces + i] =
                    match &kstate_orbit_data.orientation_mod {
                        None => kstate_orbit_data.orientation[i],
                        Some(orientation_mod) => {
                            match orientation_mod[i] {
                                0 => kstate_orbit_data.orientation[i],
                                1 => current_orbit.definition.num_orientations * 2, // TODO
                                _ => panic!("Unsupported!"),                        // TODO
                            }
                        }
                    }
                    .try_into()
                    .expect("Value too large!")
            }
        }

        PackedKState {
            packed_kpuzzle: self.clone(),
            bytes,
        }
    }
}
