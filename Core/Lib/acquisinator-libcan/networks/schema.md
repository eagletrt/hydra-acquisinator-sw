# Network JSON Schema

## Root

```
root
├── version      optional  number
├── types        optional  object
│   └── <name>             type_definition
├── attributes   optional  array<attribute_definition>
├── nodes        optional  array<node>
└── messages     required  array<message>
```

---

## type_definition

```
type_definition  (oneOf)
├── int | uint
│   ├── type     required  "int" | "uint"
│   ├── size     required  8 | 16 | 32 | 64
│   └── comment  optional  string
├── float | double | bool
│   ├── type     required  "float" | "double" | "bool"
│   └── comment  optional  string
└── enum
    ├── type     required  "enum"
    ├── comment  optional  string
    └── items    required  array
        └── enum_item
            ├── name     required  string
            ├── value    optional  integer  (auto-assigned if omitted)
            └── comment  optional  string
```

---

## attribute_definition

```
attribute_definition
├── name     required  string
├── field    optional  "global" | "node" | "message" | "signal"
├── type     required  "INT" | "HEX" | "FLOAT" | "STRING" | "ENUM"
├── default  required  number | string
└── range    optional  (absent for STRING)
    ├── INT | HEX | FLOAT
    │   ├── min    required  number
    │   └── max    required  number
    └── ENUM
        └── fields  required  array<string>
```

---

## attribute_value

```
attribute_value
├── name   required  string
└── value  required  number | string
```

---

## node

```
node
├── name        required  string
├── comment     optional  string
└── attributes  optional  array<attribute_value>
```

---

## message

```
message  (oneOf: can_id | priority)
├── name        required  string
├── can_id      oneOf     integer ≥ 0   fixed CAN ID
├── priority    oneOf     integer 0–3   auto-generated ID
├── length      optional  integer 0–8   computed if omitted
├── sender      required  string
├── comment     optional  string
├── signals     optional  array<signal>
└── attributes  optional  array<attribute_value>
```

---

## signal

```
signal  (anyOf — see cases below)
├── name        required  string
├── type        optional  string         ref to types
├── bit_start   optional  integer ≥ 0
├── length      optional  integer ≥ 1
├── endianness  optional  "BigEndian" | "LittleEndian"
├── signed      optional  bool
├── scale       optional  number
├── offset      optional  number         default = min (unsigned), 0 (signed)
├── min         optional  number
├── max         optional  number
├── unit        optional  string         must match the name unit, see units.toml
├── receivers   optional  array<string>
├── comment     optional  string
├── bitfield    optional  bool           C bitfield (e.g. uint8_t x:5;)
├── mux_id      optional  integer        only inside multiplexed[]
├── multiplexed optional  array<signal>
└── attributes  optional  array<attribute_value>
```

### anyOf cases

| Case        | Required fields                       | Builder computes            |
| ----------- | ------------------------------------- | --------------------------- |
| DBC classic | `length`, `signed`, `scale`, `offset` | `min`, `max`                |
| Designer    | `min`, `max`, `length`                | `scale`, `offset`           |
| Auto-sizing | `min`, `max`, `scale`                 | `length`                    |
| Type-only   | `type` (with size)                    | `length`, `scale`, `offset` |

### offset semantics (unsigned signals)

- omitted: `offset = min`
- explicit: `offset_total = min + offset`
