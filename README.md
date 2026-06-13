# MediGuard Clinical Prescription Verification System
# An Abstract Symmetric Bidirectional Turing Machine Simulator for Automated Clinical Auditing

------------------------------------------------------------------------------------------------
# Project Overview
The **MediGuard Clinical Prescription Verification System** is a production-grade C application that leverages formal language theory and finite automata principles to audit medical prescriptions. The system models an abstract **Symmetric Bidirectional Turing Machine Control Engine** to sequentially evaluate a structured data stream (acting as an input tape). 
The machine evaluates structural identifiers, verifies drug classifications against a local database, performs safe pediatric scaling computations, handles complex string-tokenized allergy cross-screening, and intercepts dangerous molecular cross-reactions.

------------------------------------------------------------------------------------------------
# Turing Machine Formal Architecture
The system interprets the patient's prescription data structure as an input tape, processing metrics sequentially through a series of discrete execution states using an internal control unit (`switch` matrix).

## 1. Finite State Register (TM_State)
* Q0 / Q_CHECK_ID:Initial tape head entry. Verifies string syntax, validates structural layouts, and handles numeric offset checking.
* Q_LOOKUP_DRUG1 & Q_LOOKUP_DRUG2: References the database to confirm registration, matching departmental associations, and inventory availability.
* Q_CHECK_DOSE1 & Q_CHECK_DOSE2: Executes specialized quantitative checks against safety thresholds.
* Q_CHECK_ALLERGY: Triggers multi-token substring scans across patient hypersensitivity registries.
* Q_CHECK_INTERACTION: Performs final safety validation for drug-to-drug molecular compatibility.
* Q_ACCEPT: The final accepting state indicating full validation and authorizing dispensing.

## 2. Bidirectional Tape Head Simulation
The architecture exhibits symmetric bidirectional mechanics based on the execution mode:
* Automated Batch Mode:Operates as a strict Deterministic Finite Automaton. Any single validation failure acts as a terminal condition, transitioning the engine immediately to a critical halt block (machine_halted = 1).
* Interactive Mode: Emulates a bidirectional tape rewind head. When a state verification fails, the control unit transitions to a specific rollback retry state (Q_RETRY_ID, Q_RETRY_DRUG1, etc.). This logs a diagnostic track, moves the tape head left (Move L), purges the invalid buffer cell, and prompts for updated manual console entry without crashing the runtime environment.

------------------------------------------------------------------------------------------------
# Core Clinical Validation Modules

## Module 1: Structural ID Decomposition
The 9-digit alphanumeric string payload undergoes position-sensitive parsing using string manipulation rules and character-offset mathematics:

**int year = (p->patient_id[0] - '0') * 10 + (p->patient_id[1] - '0');**

* Digits 1-2 (Year Batch): Validates the active institutional cohort (Must equal '26' for Year 2026).
* Digit 3 (Category Code): Determines clinical routing flags ('1' = Emergency, '2' = Outpatient Department (OPD)).
* Digits 4-5 (Serial Number): Numeric bounds check ensuring registration within bounds ('01' to '99').
* Digits 6-7 (Department Code): Extracted from the prescription identifier prefix to ensure cross-department compatibility ('General Medicine =12', 'Orthopedics =14').
* Digit 8-9 (Medical Record):Will Fetch medical record of the existing patient.

## Module 2: Generic Drug Verification & Formulary Lookup
References the internal database to ensure that prescribed drugs are registered within the system, match the active clinic department code, and contain sufficient quantity:
* Compares input keys against the formulary using substring analysis (strstr).
* Ensures that department numbers line up perfectly (e.g., a drug belonging to department 12 cannot be verified if the prescription code specifies department 14).
* Halts execution immediately if the available stock counter drops to 0.

## Module 3: Quantitative Dosage Scaling (Pediatric Optimization Rule)
The engine executes dynamic quantitative bounds checking based on physical patient constraints:
* Standard Adult Profile: Enforces the absolute maximum safe dosage specified in the formulary.
* Pediatric Safety Profile: **Triggered automatically if patient_weight < 20 kg**. The engine dynamically halves (divides by 2) the maximum safe dosage floor in temporary memory before evaluating the input request, preventing toxicity risks in low-weight profiles.

## Module 4: Anaphylactic Cross-Screening & Tokenization
* Multi-Allergen Parsing: Uses strtok() pointer loops to split comma-separated allergy entries into distinct validation strings.
* Substring Pattern Evaluation: Uses strstr() pattern matching to flag identical compounds or active component variations.
* Cross-Reactivity Logic: Implements smart cross-sensitivity overrides. If an allergy string contains Penicillin tags (PENC or PENICILLIN), the system intercepts and blocks downstream variations like Amoxicillin (AMOX), halting tape execution.

## Module 5: Molecular Interaction Screening
Protects patient health by enforcing zero-tolerance blocks against high-alert, overlapping drug pairs:
1. IBU200 + WARF5: Prohibited due to high internal bleeding hazards.
2. PENC250 + AMOX500: Blocked due to redundant therapeutic mechanism configurations.

------------------------------------------------------------------------------------------------
# Pharmacy Formulary Registry Database
The processing engine references a static central registry modeling a real-world inventory management framework:
(Registered Drug Code)	(Department Association)	(Max Safe Threshold (Adult))	(Current Base Inventory)
1. AMOX500	12 (General Medicine)	1000 mg	50 Units (In Stock)
2. PENC250	12 (General Medicine)	500 mg	0 Units (Out of Stock)
3. IBU200	14 (Orthopedics)	800 mg	120 Units (In Stock)
4. WARF5	14 (Orthopedics)	500 mg	80 Units (In Stock)

------------------------------------------------------------------------------------------------
# Compilation & Deployment
**Compilation**
Compile the project using standard 64-bit GCC or any ANSI C-compliant compiler platform like DevC++

------------------------------------------------------------------------------------------------
# Interactive Manual Entry Guide (Mode 1)
When selecting **Mode [1]**, input the following 7 parameters sequentially:
1. Combined 9-Digit ID:e.g., '261051488' (Batch 26, Dept 14).
2. Primary Drug Code: Case-insensitive formulary key (e.g., 'AMOX500', 'IBU200').
3. Primary Dose (mg): Integer value for drug strength.
4. Secondary Drug Code: Second medication key or 'NONE'.
5. Secondary Dose (mg): Secondary strength integer (Enter 0 if empty).
6. Patient Weight (kg): Used for dynamic pediatric scaling thresholds.
7. Allergy Disclosures: Comma-separated list (e.g., PENC,IBU) or NONE.

# Automated Batch Testing Matrix (Mode 2)
Selecting **Mode [2]** runs 5 automated test cases across the validation pipeline:

# Test Case 1: Baseline Optimal Profile (APPROVED)
Input: ID: 261051488 | IBU200 (400mg) | Weight: 70kg | Allergy: NONE

Path: Q_CHECK_ID → Q_LOOKUP_DRUG1 → Q_CHECK_DOSE1 → Q_CHECK_ALLERGY → Q_ACCEPT

# Test Case 2: Outdated Batch Exception (REJECTED)
Input: ID: 251051488 | IBU200 (400mg) | Weight: 70kg | Allergy: NONE

Halt Reason: Invalid Year '2025' at state Q_CHECK_ID. Only batch 2026 allowed.

# Test Case 3: Supply Chain Exhaustion Block (REJECTED)
Input: ID: 261051288 | PENC250 (250mg) | Weight: 65kg | Allergy: NONE

Halt Reason: Found valid record at Q_LOOKUP_DRUG1 but available stock is 0.

# Test Case 4: Pediatric Toxicity Overdose Exception (REJECTED)
Input: ID: 261051288 | AMOX500 (650mg) | Weight: 14kg | Allergy: NONE

Halt Reason: Weight < 20kg at Q_CHECK_DOSE1 halves max limit to 500mg. Input 650mg violates safety.

# Test Case 5: Lethal Molecular Cross-Reaction Guard (REJECTED)
Input: ID: 261051488 | IBU200 (200mg) + WARF5 (150mg) | Weight: 75kg | Allergy: NONE

Halt Reason: State Q_CHECK_INTERACTION triggers critical block for forbidden pair [IBU200 + WARF5].
