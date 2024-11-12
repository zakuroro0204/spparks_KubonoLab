import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import linregress

class DiffusionAnalysis:
    def __init__(self, filename):
        """
        Parameters:
        filename : dumpファイルのパス
        """
        self.filename = filename
        self.lattice_constant = 0.384  # nm
        self.frequency = 1e12          # Hz
        
    def read_dump_file(self):
        """SPPARKSのdumpファイルを読み込む"""
        data = []
        current_timestep = []
        with open(self.filename, 'r') as f:
            lines = f.readlines()
            i = 0
            while i < len(lines):
                if 'ITEM: TIMESTEP' in lines[i]:
                    if current_timestep:
                        data.append(np.array(current_timestep))
                        current_timestep = []
                    i += 2  # Skip timestamp line
                elif 'ITEM: NUMBER OF ATOMS' in lines[i]:
                    i += 2  # Skip number of atoms line
                elif 'ITEM: BOX BOUNDS' in lines[i]:
                    i += 4  # Skip box bounds
                elif 'ITEM: ATOMS' in lines[i]:
                    i += 1
                else:
                    # id site x y z の形式で読み込み
                    values = lines[i].split()
                    current_timestep.append([float(v) for v in values])
                    i += 1
            if current_timestep:
                data.append(np.array(current_timestep))
        return data

    def calculate_msd(self, data):
        """平均二乗変位（MSD）の計算
        
        Returns:
        times : 時間配列 [ps]
        msd   : 平均二乗変位 [nm^2]
        """
        times = []
        msd = []
        
        # 初期位置を保存
        initial_positions = {}
        for particle in data[0]:
            particle_id = int(particle[0])
            x, y = particle[2:4]
            initial_positions[particle_id] = (x, y)
        
        # 各時間でのMSDを計算
        for timestep, frame in enumerate(data):
            displacement_sq = []
            for particle in frame:
                particle_id = int(particle[0])
                if particle_id in initial_positions:
                    x, y = particle[2:4]
                    x0, y0 = initial_positions[particle_id]
                    # 周期境界条件を考慮した変位の計算
                    dx = (x - x0) * self.lattice_constant
                    dy = (y - y0) * self.lattice_constant
                    dr2 = dx*dx + dy*dy
                    displacement_sq.append(dr2)
            
            time = timestep * 100.0 / self.frequency  # ps単位
            times.append(time)
            msd.append(np.mean(displacement_sq))
            
        return np.array(times), np.array(msd)

    def calculate_diffusion_coefficient(self, times, msd):
        """拡散係数の計算
        
        Returns:
        D : 拡散係数 [m^2/s]
        """
        # 直線フィッティング
        slope, intercept, r_value, p_value, std_err = linregress(times, msd)
        
        # D = slope/4 （2次元の場合）
        D = slope / (4 * 1e12)  # ps -> s の変換を含む
        return D * 1e-18  # nm^2/ps -> m^2/s

    def plot_msd(self, times, msd):
        """MSDのプロット"""
        plt.figure(figsize=(10, 6))
        plt.plot(times, msd, 'o-', label='MSD')
        plt.xlabel('Time (ps)')
        plt.ylabel('MSD (nm$^2$)')
        plt.title('Mean Square Displacement vs Time')
        plt.grid(True)
        plt.legend()
        plt.savefig('msd_plot.png')
        plt.close()

    def analyze(self):
        """完全な解析を実行"""
        print("読み込みを開始...")
        data = self.read_dump_file()
        
        print("MSDを計算中...")
        times, msd = self.calculate_msd(data)
        
        print("拡散係数を計算中...")
        D = self.calculate_diffusion_coefficient(times, msd)
        
        print(f"拡散係数: {D:.2e} m^2/s")
        
        print("プロットを生成中...")
        self.plot_msd(times, msd)
        
        return D, times, msd

# 使用例
if __name__ == "__main__":
    analyzer = DiffusionAnalysis("dump.msd.*")
    D, times, msd = analyzer.analyze()
    
    # 理論値との比較
    Ed = 0.74  # eV
    kT = 8.617e-5 * 297  # eV
    D_theoretical = (0.384e-9)**2 * 1e12 * np.exp(-Ed/kT)
    
    print(f"理論値との比較:")
    print(f"シミュレーション結果: {D:.2e} m^2/s")
    print(f"理論値: {D_theoretical:.2e} m^2/s")
    print(f"相対誤差: {abs(D-D_theoretical)/D_theoretical*100:.1f}%")