import os
import time
import sys

def simulate_attack(target_dir):
    if not os.path.exists(target_dir):
        os.makedirs(target_dir)
        print(f"[+] Created target directory: {target_dir}")

    print(f"[!] Starting Ransomware Attack Simulation on: {target_dir}")
    print("[!] Writing 15 high-entropy files rapidly...")

    # Write 15 random files in under 1 second
    for i in range(1, 16):
        filename = f"ransomware_mock_{i}.locked"
        filepath = os.path.join(target_dir, filename)
        
        # High entropy payload: completely random bytes
        payload = os.urandom(1024) 
        
        with open(filepath, "wb") as f:
            f.write(payload)
            
        print(f"  [{i}/15] Encrypted: {filepath}")
        # Small sleep to simulate execution flow
        time.sleep(0.02) 

    print("[+] Simulation Finished. Check backend log for RANSOMWARE PANIK ALARMI!")

if __name__ == "__main__":
    target = "./test_folder"
    if len(sys.argv) > 1:
        target = sys.argv[1]
    simulate_attack(target)
