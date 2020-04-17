-- Based off the cave demo settings, as found in a screen shot here: https://github.com/Auburns/FastNoiseSIMD/issues/37
return {
	priority = 100,
	init = function(self, seed)
		self.caveNoise = noise.new(noiseType.Cellular, seed, .003)
		self.caveNoise:setCellularReturnType(cellularReturnType.Distance2Cave)
		self.caveNoise:setCellularJitter(.3)
		self.caveNoise:setPerturbType(perturbType.GradientFractal)
		self.caveNoise:setPerturbAmp(0.3)
		self.caveNoise:setPerturbFrequency(3)
		self.caveNoise:setPerturbFractalOctaves(2)
		self.caveNoise:setPerturbFractalLacunarity(12)
		self.caveNoise:setPerturbFractalGain(.08)
	end,
	generateChunk = function(self, world, chunk, x, y, z)
		if y > -5 then return end
		local chunkSize = world.renderers.voxel.chunkSize
		local noiseSet = self.caveNoise:getNoiseSet(x, y, z, chunkSize)
		for point,density in pairs(noiseSet) do
			if y < -6 then
				if density > 0.88 then
					chunk.blocks[point - 1] = nil
				end
			else
				-- Taper off over the course of one vertical chunk
				local internalY = (point - 1) % (chunkSize * chunkSize) / chunkSize
				if density - internalY * 0.88/ chunkSize > 0.88 then
					chunk.blocks[point - 1] = nil
				end
			end
		end
	end
}
