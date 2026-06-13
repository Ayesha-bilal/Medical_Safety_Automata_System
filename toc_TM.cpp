#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// states
typedef enum {
    Q0,
    Q_CHECK_ID, Q_RETRY_ID,
    Q_LOOKUP_DRUG1, Q_RETRY_DRUG1,
    Q_CHECK_DOSE1, Q_RETRY_DOSE1,
    Q_LOOKUP_DRUG2, Q_RETRY_DRUG2,
    Q_CHECK_DOSE2, Q_RETRY_DOSE2,
    Q_CHECK_ALLERGY, Q_RETRY_ALLERGY,
    Q_CHECK_INTERACTION, Q_RETRY_INTERACTION,
    Q_ACCEPT
} TM_State;

//Prescription
typedef struct {
    char combined_id[20];
    char patient_id[10];
    char prescription_id[10];
    char drug1[20];
    char drug2[20];
    int dose1;
    int dose2;
    int patient_weight;
    char allergy[100];
} Prescription;

// Static Database
typedef struct {
    char drug_name[20];
    int dept_code;
    int max_safe_dosage;
    int stock_available; 
} DrugRecord;

//drugrecord 
DrugRecord local_database[4] = {
    {"AMOX500", 12, 1000, 50}, 
    {"PENC250", 12, 500, 0},   
    {"IBU200", 14, 800, 120},
    {"WARF5", 14, 500, 80}
};

char error_reason[150];

// Helper Input Parsers
//1. check if the number is numeric or not
int is_numeric_string(const char* str) {
    int len = strlen(str);
    if (len == 0) {
    	return 0;
	}
    for (int i = 0; i < len; i++) {
        if (!isdigit((unsigned char)str[i])){
        	 return 0;
		}
    }
    return 1;
}

//2.converting to uppercase becuase the database is in uppercase
void normalize_to_uppercase(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper((unsigned char)str[i]);
    }
}

// Tape Module 1: Comprehensive ID Parsing
int validate_id(Prescription* p) {
    int length = strlen(p->combined_id);
    //key should be of length 9
    if (length != 9) {
        sprintf(error_reason, "ID Format Error: Must be exactly 9 digits. (Length: %d)", length);
        return 0;
    }
    if (!is_numeric_string(p->combined_id)) {
        strcpy(error_reason, "ID Format Error: Contains non-numeric characters.");
        return 0;
    }
    
    //first 5 digit=patient_id
    strncpy(p->patient_id, p->combined_id, 5); 
    p->patient_id[5] = '\0';
    //last 4 dijit=prescription_id
    strncpy(p->prescription_id, p->combined_id + 5, 4);
    p->prescription_id[4] = '\0';
    
    //checking year : should be 26
    int year = (p->patient_id[0] - '0') * 10 + (p->patient_id[1] - '0');
    if (year != 26) {
        sprintf(error_reason, "Patient ID Segment Error: Invalid Year '20%02d'. Only batch 2026 allowed.", year);
        return 0;
    }
    
    //checking category : should be 1(emergency) & 2(OPD)
    char category = p->patient_id[2];
    if (category != '1' && category != '2') {
        sprintf(error_reason, "Patient ID Segment Error: Invalid Category Code '%c' (Use 1 or 2).", category);
        return 0;
    } 
    //checking serial : should be in between 0 to 99
    int serial = atoi(&p->patient_id[3]);
    if (serial < 1 || serial > 99) {
        sprintf(error_reason, "Patient ID Segment Error: Serial '%02d' out of bounds (01-99).", serial);
        return 0;
    }
    
    //checking dept code= should be 12(general medicine) or 14(orthopadics)
    int dept_code = (p->prescription_id[0] - '0') * 10 + (p->prescription_id[1] - '0');
    for (int i = 0; i < 4; i++) {
        if (local_database[i].dept_code == dept_code) return 1;
    }
    sprintf(error_reason, "Prescription ID Segment Error: Department '%02d' unregistered.", dept_code);
    return 0;
}

// Tape Module 2: Generic Drug Verification (Stock, Registration & Formulary Lookup)
int lookup_drug(char* drug_name, int dept_code, int* db_index) {
    if (strcmp(drug_name, "NONE") == 0 || strlen(drug_name) == 0) return 1;
    
    for (int i = 0; i < 4; i++) {
        if (strstr(local_database[i].drug_name, drug_name) != NULL) {
            *db_index = i;
            if (local_database[i].dept_code != dept_code) {
                sprintf(error_reason, "Formulary Error: Drug '%s' belongs to Dept %d, not prescribed Dept %d.", local_database[i].drug_name, local_database[i].dept_code, dept_code);
                return 0;
            }
            if (local_database[i].stock_available <= 0) {
                sprintf(error_reason, "Inventory Error: Drug '%s' is OUT OF STOCK.", local_database[i].drug_name);
                return 0;
            }
            strcpy(drug_name, local_database[i].drug_name);
            return 1;
        }
    }
    sprintf(error_reason, "Formulary Error: Drug Code '%s' not found.", drug_name);
    return 0;
}

// Tape Module 3: Quantitative Dosage Optimization (Standard vs Pediatric Halving)
int verify_dosage(int dose, int patient_weight, int db_index, const char* drug_name) {
    if (db_index == -1) 
	return 1; 
    
    int max_limit = local_database[db_index].max_safe_dosage;
    int original_limit = max_limit;
    int pediatric_applied = 0;
    if (patient_weight < 20) { 
        max_limit /= 2; 
		pediatric_applied = 1; 
    } 
    if (dose > max_limit) {
        if (pediatric_applied) {
            sprintf(error_reason, "Dosage Error: Pediatric (<20kg) for '%s'. Safe max halved from %dmg to %dmg. Input: %dmg.", drug_name, original_limit, max_limit, dose);
        } else {
            sprintf(error_reason, "Dosage Error: Safety limit for '%s' is %dmg. Input: %dmg.", drug_name, max_limit, dose);
        }
        return 0;
    }
    return 1;
}

// Tape Module 4: Anaphylactic Cross-Screening with Medical Family Rules
int check_allergy(Prescription* p) {
    if (strcmp(p->allergy, "NONE") == 0 || strlen(p->allergy) == 0) return 1;
    
    char temp_allergy[100]; 
    strcpy(temp_allergy, p->allergy);
    
    char *token = strtok(temp_allergy, ",");
    while (token != NULL) {
        // 1. Direct exact or substring match (e.g., IBU allergy blocks IBU200)
        if (strstr(p->drug1, token) != NULL) {
            sprintf(error_reason, "Allergy Violation: Compound element matches profile ['%s'] in Primary Drug.", token);
            return 0;
        }
        if (strcmp(p->drug2, "NONE") != 0 && strlen(p->drug2) > 0) {
            if (strstr(p->drug2, token) != NULL) {
                sprintf(error_reason, "Allergy Violation: Compound element matches profile ['%s'] in Secondary Drug.", token);
                return 0;
            }
        }
        
        // 2. Smart Cross-Reactivity Rule: Penicillin allergy explicitly blocks Amoxicillin (AMOX)
        if (strcmp(token, "PENC") == 0 || strcmp(token, "PENICILLIN") == 0) {
            if (strstr(p->drug1, "AMOX") != NULL) {
                strcpy(error_reason, "Cross-Allergy Violation: Patient has Penicillin allergy. Amoxicillin (AMOX) is strictly prohibited.");
                return 0;
            }
            if (strstr(p->drug2, "AMOX") != NULL) {
                strcpy(error_reason, "Cross-Allergy Violation: Patient has Penicillin allergy. Amoxicillin (AMOX) is strictly prohibited.");
                return 0;
            }
        }
        
        token = strtok(NULL, ",");
    }
    return 1;
}

// Tape Module 5: Molecular Interaction Screening
int check_interaction(char* drug1, char* drug2) {
    if (strcmp(drug2, "NONE") == 0 || strlen(drug2) == 0) 
	return 1;
    if ((strcmp(drug1, "IBU200") == 0 && strcmp(drug2, "WARF5") == 0) || (strcmp(drug1, "WARF5") == 0 && strcmp(drug2, "IBU200") == 0)) {
        strcpy(error_reason, "Molecular Conflict: High-alert cross-reaction [IBU200 + WARF5]. High internal bleeding risk.");
        return 0;
    } 
    if ((strcmp(drug1, "PENC250") == 0 && strcmp(drug2, "AMOX500") == 0) || (strcmp(drug1, "AMOX500") == 0 && strcmp(drug2, "PENC250") == 0)) {
        strcpy(error_reason, "Therapeutic Conflict: Overlapping mechanism redundant block [PENC250 + AMOX500].");
        return 0;
    }
    return 1;
}

// Symmetric Bidirectional Turing Machine Engine
void run_clinical_engine(Prescription p, int is_interactive) {
    TM_State currentState = Q0;
    int db_idx1 = -1, db_idx2 = -1;
    int dept_code = 0;
    int machine_halted=0;
    strcpy(error_reason, "No errors detected.");

    char s_id[25] = "PENDING",     s_drug1[25] = "PENDING", 
         s_dose1[25] = "PENDING",  s_drug2[25] = "PENDING", 
         s_dose2[25] = "PENDING",  s_allg[25] = "PENDING",   
         s_int[25] = "PENDING";

    while (currentState != Q_ACCEPT && !machine_halted) {
        switch (currentState) {
            case Q0: 
			currentState = Q_CHECK_ID; 
			break;
            
            case Q_CHECK_ID:
                if (validate_id(&p)) {
                    strcpy(s_id, "PASSED"); 
                    dept_code = (p.prescription_id[0] - '0') * 10 + (p.prescription_id[1] - '0');
                    currentState = Q_LOOKUP_DRUG1; 
                } else {
                    if (is_interactive) {
					 strcpy(s_id, "FAILED (RETRYING)"); 
					 currentState = Q_RETRY_ID; 
					 }
                    else {
					 strcpy(s_id, "FAILED");
					  machine_halted=1; 
					  }
                }
                break;

            case Q_RETRY_ID:
                printf("\n[Tape Head <- Move L (Wiping Invalid ID Cell)]\nCRITICAL ENGINE ERROR: %s\nPlease enter valid 9-Digit Combined ID: ", error_reason);
                scanf("%s", p.combined_id);
                currentState = Q_CHECK_ID;
                break;
                
            case Q_LOOKUP_DRUG1:
                if (lookup_drug(p.drug1, dept_code, &db_idx1)) {
                    strcpy(s_drug1, "PASSED"); 
                    currentState = Q_CHECK_DOSE1; 
                } else {
                    if (is_interactive) {
				   strcpy(s_drug1, "FAILED (RETRYING)"); 
				   currentState = Q_RETRY_DRUG1; 
				   }
                    else { 
					strcpy(s_drug1, "FAILED"); 
					machine_halted=1;
					 }
                }
                break;

            case Q_RETRY_DRUG1:
                printf("\n[Tape Head <- Move L (Purging Primary Drug Input)]\nCRITICAL ENGINE ERROR: %s\nEnter alternate Primary Drug Code: ", error_reason);
                scanf("%s", p.drug1);
                normalize_to_uppercase(p.drug1);
                currentState = Q_LOOKUP_DRUG1;
                break;
                
            case Q_CHECK_DOSE1:
                if (verify_dosage(p.dose1, p.patient_weight, db_idx1, p.drug1)) {
                    strcpy(s_dose1, "PASSED"); 
                    currentState = Q_LOOKUP_DRUG2; 
                } else {
                    if (is_interactive) {
					 strcpy(s_dose1, "FAILED (RETRYING)"); 
					 currentState = Q_RETRY_DOSE1; 
					 }
                    else { 
					strcpy(s_dose1, "FAILED"); 
					machine_halted=1; 
					}
                }
                break;

            case Q_RETRY_DOSE1:
                printf("\n[Tape Head <- Move L (Resetting Primary Toxic Dose Floor)]\nCRITICAL ENGINE ERROR: %s\nEnter safe Primary dosage (mg): ", error_reason);
                scanf("%d", &p.dose1);
                currentState = Q_CHECK_DOSE1;
                break;

            case Q_LOOKUP_DRUG2:
                if (strcmp(p.drug2, "NONE") == 0 || strlen(p.drug2) == 0) {
                    strcpy(s_drug2, "SKIPPED");
                    strcpy(s_dose2, "SKIPPED");
                    currentState = Q_CHECK_ALLERGY;
                } else {
                    if (lookup_drug(p.drug2, dept_code, &db_idx2)) {
                        strcpy(s_drug2, "PASSED");
                        currentState = Q_CHECK_DOSE2;
                    } else {
                        if (is_interactive) {
						 strcpy(s_drug2, "FAILED (RETRYING)");
						  currentState = Q_RETRY_DRUG2; 
						  }
                        else {
						 strcpy(s_drug2, "FAILED"); 
						machine_halted=1; 
						}
                    }
                }
                break;

            case Q_RETRY_DRUG2:
                printf("\n[Tape Head <- Move L (Purging Secondary Drug Input)]\nCRITICAL ENGINE ERROR: %s\nEnter alternate Secondary Drug Code (or 'NONE'): ", error_reason);
                scanf("%s", p.drug2);
                normalize_to_uppercase(p.drug2);
                currentState = Q_LOOKUP_DRUG2;
                break;

            case Q_CHECK_DOSE2:
                if (verify_dosage(p.dose2, p.patient_weight, db_idx2, p.drug2)) {
                    strcpy(s_dose2, "PASSED");
                    currentState = Q_CHECK_ALLERGY;
                } else {
                    if (is_interactive) {
					 strcpy(s_dose2, "FAILED (RETRYING)");
					  currentState = Q_RETRY_DOSE2; 
					  }
                    else {
					 strcpy(s_dose2, "FAILED"); 
					 machine_halted=1; 
					 }
                }
                break;

            case Q_RETRY_DOSE2:
                printf("\n[Tape Head <- Move L (Resetting Secondary Toxic Dose Floor)]\nCRITICAL ENGINE ERROR: %s\nEnter safe Secondary dosage (mg): ", error_reason);
                scanf("%d", &p.dose2);
                currentState = Q_CHECK_DOSE2;
                break;
                
            case Q_CHECK_ALLERGY:
                if (check_allergy(&p)) {
                    strcpy(s_allg, "PASSED");
                    currentState = Q_CHECK_INTERACTION; 
                } else {
                    if (is_interactive) {
					 strcpy(s_allg, "FAILED (RETRYING)");
					  currentState = Q_RETRY_ALLERGY; 
					  }
                    else { 
					strcpy(s_allg, "FAILED"); 
					machine_halted=1; 
					}
                }
                break;

            case Q_RETRY_ALLERGY:
                printf("\n[Tape Head <- Move L (Intercepting Allergens)]\nCRITICAL ENGINE ERROR: %s\nEnter updated allergy profile or 'NONE': ", error_reason);
                scanf("%s", p.allergy);
                normalize_to_uppercase(p.allergy);
                currentState = Q_CHECK_ALLERGY;
                break;
                
            case Q_CHECK_INTERACTION:
                if (check_interaction(p.drug1, p.drug2)) {
                    strcpy(s_int, "PASSED");
                    currentState = Q_ACCEPT;  
                } else {
                    if (is_interactive) {
					 strcpy(s_int, "FAILED (RETRYING)");
					  currentState = Q_RETRY_INTERACTION; 
					  }
                    else { 
					strcpy(s_int, "FAILED"); 
					machine_halted=1; 
					}
                }
                break;

            case Q_RETRY_INTERACTION:
                printf("\n[Tape Head <- Move L (Splitting Reactive Compounds)]\nCRITICAL ENGINE ERROR: %s\nEnter alternate Secondary Drug Code (or 'NONE'): ", error_reason);
                scanf("%s", p.drug2);
                normalize_to_uppercase(p.drug2);
                currentState = Q_CHECK_INTERACTION;
                break;
                
           default: 
		   machine_halted = 1; 
		   break;
        }
    }

    printf("+----------------------------------------------------+\n");
    printf("  1. Intake ID Verification     :  %s\n", s_id);
    printf("  2. Primary Drug Authorization :  %s\n", s_drug1);
    printf("  3. Primary Quantitative Dose :  %s\n", s_dose1);
    printf("  4. Secondary Drug Auth & Stock:  %s\n", s_drug2);
    printf("  5. Secondary Quantitative Dose:  %s\n", s_dose2);
    printf("  6. Allergy Safety Screening   :  %s\n", s_allg);
    printf("  7. Molecular Interaction Check:  %s\n", s_int);
    printf("+----------------------------------------------------+\n");
    if (machine_halted) {
        printf("  CRITICAL HALT BLOCK: %s\n", error_reason);
        printf("+----------------------------------------------------+\n");
    }
    printf("  Patient Regimen: ID [%s] | Order: %s [%dmg] + %s [%dmg]\n", p.combined_id, p.drug1, p.dose1, p.drug2, p.dose2);
    printf("  DISPENSE DECISION  : %s\n", (currentState == Q_ACCEPT) ? "APPROVED" : "REJECTED");
    printf("+----------------------------------------------------+\n\n");
}

int main() {
    int choice;
    printf("======================================================\n");
    printf("        MEDIGUARD AUTOMATA SYSTEM SIMULATOR           \n");
    printf("======================================================\n");
    printf("  Select Evaluation Mode:\n");
    printf("  [1] Interactive Mode (Manual Console Entry)\n");
    printf("  [2] Automated Simulation (Execute 5 Mandatory Test Cases)\n");
    printf("------------------------------------------------------\n");
    printf("  Enter Selection (1 or 2): ");
    if (scanf("%d", &choice) != 1) return 1;
    printf("\n");

    if (choice == 1) {
        Prescription manual_p;
        printf("+----------------------------------------------------+\n");
        printf("|         MANUAL PRESCRIPTION FORM PROCESSING        |\n");
        printf("+----------------------------------------------------+\n");
        
        printf("  Combined 9-Digit ID   : "); scanf("%s", manual_p.combined_id);
        printf("  Primary Drug Code     : "); scanf("%s", manual_p.drug1);
        printf("  Primary Dose (mg)     : "); if(scanf("%d", &manual_p.dose1) != 1) return 1;
        printf("  Secondary Drug Code   : "); scanf("%s", manual_p.drug2);
        printf("  Secondary Dose (mg)   : "); if(scanf("%d", &manual_p.dose2) != 1) return 1;
        printf("  Patient Weight (kg)   : "); if(scanf("%d", &manual_p.patient_weight) != 1) return 1;
        printf("  Allergy Disclosures   : "); scanf("%s", manual_p.allergy);
        printf("+----------------------------------------------------+\n\n");
        
        normalize_to_uppercase(manual_p.drug1);
        normalize_to_uppercase(manual_p.drug2);
        normalize_to_uppercase(manual_p.allergy);

        printf("+----------------------------------------------------+\n");
        printf("|           CLINICAL INSPECTION DASHBOARD             |\n");
        run_clinical_engine(manual_p, 1); 
    } 
    else if (choice == 2) {
        printf("======================================================\n");
        printf("    RUNNING 5 COMPREHENSIVE AUTOMATED TEST CASES      \n");
        printf("======================================================\n\n");

        // 5 Test Cases mapped explicitly for dual-drug quantitative screening
        Prescription batch[5] = {
            {"261051488", "", "", "IBU200", "NONE", 400, 0, 70, "NONE"},
            {"251051488", "", "", "IBU200", "NONE", 400, 0, 70, "NONE"},
            {"261051288", "", "", "PENC250", "NONE", 250, 0, 65, "NONE"},
            {"261051288", "", "", "AMOX500", "NONE", 650, 0, 14, "NONE"},
            {"261051488", "", "", "IBU200", "WARF5", 200, 150, 75, "NONE"}
        };

        for (int i = 0; i < 5; i++) {
            printf("[TEST CASE %d/5] Processing Tape Evaluation...\n", i + 1);
            printf("  >> Input Tape State -> ID: %s | Drug1: %s [%dmg] | Drug2: %s [%dmg] | Weight: %dkg | Allergy: %s\n",
                   batch[i].combined_id, batch[i].drug1, batch[i].dose1, batch[i].drug2, batch[i].dose2, batch[i].patient_weight, batch[i].allergy);
            
            normalize_to_uppercase(batch[i].drug1);
            normalize_to_uppercase(batch[i].drug2);
            normalize_to_uppercase(batch[i].allergy);
            run_clinical_engine(batch[i], 0); 
        }
    } 
    else {
        printf("Invalid choice selection. Exiting.\n");
    }
    return 0;
}
